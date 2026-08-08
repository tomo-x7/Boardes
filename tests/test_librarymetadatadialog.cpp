#include <QLineEdit>
#include <QTest>

#include "ui/librarymetadatadialog.h"
#include "ui/licensepickerwidget.h"

class TestLibraryMetadataDialog : public QObject {
	Q_OBJECT

private slots:
	void roundTripsEditableFields();
	void doesNotTouchIdOrContents();
	void licenseChangeRederivesRedistributionRule();
	void createModeAppliesEnteredId();
};

void TestLibraryMetadataDialog::roundTripsEditableFields() {
	Library lib;
	lib.id = "my-library";
	lib.name = QStringLiteral("元の名前");
	lib.version = "1.0.0";
	lib.author = QStringLiteral("作者A");
	lib.authorUrl = "https://a.example.com";
	lib.homepage = "https://home.example.com";
	lib.description = QStringLiteral("説明文");
	lib.license.kind = LicenseKind::MIT;

	LibraryMetadataDialog dialog;
	dialog.setLibrary(lib);

	Library result = lib;  // 既存の id/parts 等を保持したまま applyTo で上書きされる想定
	result.name.clear();
	dialog.applyTo(result);

	QCOMPARE(result.name, lib.name);
	QCOMPARE(result.version, lib.version);
	QCOMPARE(result.author, lib.author);
	QCOMPARE(result.authorUrl, lib.authorUrl);
	QCOMPARE(result.homepage, lib.homepage);
	QCOMPARE(result.description, lib.description);
	QCOMPARE(static_cast<int>(result.license.kind), static_cast<int>(LicenseKind::MIT));
}

void TestLibraryMetadataDialog::doesNotTouchIdOrContents() {
	Library lib;
	lib.id = "my-library";
	auto part = std::make_shared<Part>();
	part->id = "P1";
	lib.parts.insert(part->id, part);

	LibraryMetadataDialog dialog;
	dialog.setLibrary(lib);  // Edit モードになる (id 欄は編集不可、applyTo も id を触らない)
	dialog.applyTo(lib);

	QCOMPARE(lib.id, QStringLiteral("my-library"));
	QCOMPARE(lib.parts.size(), 1);
	QVERIFY(lib.parts.contains("P1"));
}

void TestLibraryMetadataDialog::licenseChangeRederivesRedistributionRule() {
	Library lib;
	lib.id = "my-library";
	lib.license.kind = LicenseKind::AllRightsReserved;
	lib.redistribution = redistributionRuleFor(LicenseKind::AllRightsReserved);
	QVERIFY(!lib.redistribution.allowed);

	LibraryMetadataDialog dialog;
	dialog.setLibrary(lib);
	// ダイアログの LicensePickerWidget を直接操作してライセンスを変更する。
	auto *picker = dialog.findChild<LicensePickerWidget *>();
	QVERIFY(picker != nullptr);
	LicenseInfo newLicense;
	newLicense.kind = LicenseKind::CC_BY_4_0;
	picker->setLicense(newLicense);

	dialog.applyTo(lib);
	QCOMPARE(static_cast<int>(lib.license.kind), static_cast<int>(LicenseKind::CC_BY_4_0));
	QVERIFY(lib.redistribution.allowed);
	QVERIFY(lib.redistribution.attributionRequired);
}

void TestLibraryMetadataDialog::createModeAppliesEnteredId() {
	LibraryMetadataDialog dialog;  // 既定は Create モード
	auto *idEdit = dialog.findChild<QLineEdit *>();
	QVERIFY(idEdit != nullptr);
	idEdit->setText(QStringLiteral("com.example.new-lib"));
	QCOMPARE(dialog.enteredId(), QStringLiteral("com.example.new-lib"));

	Library lib;
	lib.id = QStringLiteral("should-be-overwritten");
	dialog.applyTo(lib);
	QCOMPARE(lib.id, QStringLiteral("com.example.new-lib"));
}

QTEST_MAIN(TestLibraryMetadataDialog)
#include "test_librarymetadatadialog.moc"
