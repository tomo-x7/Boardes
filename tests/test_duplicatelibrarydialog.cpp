#include <QApplication>
#include <QLineEdit>
#include <QTest>
#include <QTimer>
#include <QWidget>

#include "ui/duplicatelibrarydialog.h"
#include "ui/licensepickerwidget.h"

class TestDuplicateLibraryDialog : public QObject {
	Q_OBJECT

private slots:
	void prefillsDifferFromSource();
	void licenseSelectionIsNeverRestrictedBySource();
	void customLicenseInheritsSourceRedistributionAsStartingPoint();
	void validationBlocksSameValuesAsSource();
	void validationPassesWhenAllFieldsDiffer();

private:
	static Library makeSource(LicenseKind kind, RedistributionRule rule);
};

Library TestDuplicateLibraryDialog::makeSource(LicenseKind kind, RedistributionRule rule) {
	Library lib;
	lib.id = "source-lib";
	lib.name = QStringLiteral("元ライブラリ");
	lib.author = QStringLiteral("元の作者");
	lib.version = "1.0.0";
	lib.license.kind = kind;
	lib.redistribution = rule;
	return lib;
}

void TestDuplicateLibraryDialog::prefillsDifferFromSource() {
	const Library source = makeSource(LicenseKind::MIT, redistributionRuleFor(LicenseKind::MIT));
	DuplicateLibraryDialog dialog(source);
	const auto spec = dialog.spec();
	QVERIFY(spec.newId != source.id);
	QVERIFY(spec.newName != source.name);
	QCOMPARE(spec.newVersion, QStringLiteral("1.0.0"));  // バージョンだけは元と偶然同じ既定値
}

void TestDuplicateLibraryDialog::licenseSelectionIsNeverRestrictedBySource() {
	// 派生ライセンス強制の仕組みは廃止した (Phase 14)。元が再配布不可 (例: PasS互換) でも、
	// 複製物のライセンスは自由に選べる — 独立した創作物として利用者の責任で設定する。
	RedistributionRule rule;
	rule.allowed = false;
	const Library source = makeSource(LicenseKind::Custom, rule);

	DuplicateLibraryDialog dialog(source);
	auto *picker = dialog.findChild<LicensePickerWidget *>();
	QVERIFY(picker != nullptr);
	LicenseInfo tryMit;
	tryMit.kind = LicenseKind::MIT;
	picker->setLicense(tryMit);
	QCOMPARE(static_cast<int>(picker->license().kind), static_cast<int>(LicenseKind::MIT));
}

void TestDuplicateLibraryDialog::customLicenseInheritsSourceRedistributionAsStartingPoint() {
	// 元が Custom ライセンスなら、その再配布ルールをチェックボックスの初期値として
	// 引き継いでおく (複製後すぐに何もかも初期化されるのを避けるため)。
	RedistributionRule rule;
	rule.allowed = true;
	rule.attributionRequired = true;
	const Library source = makeSource(LicenseKind::Custom, rule);

	DuplicateLibraryDialog dialog(source);
	auto *picker = dialog.findChild<LicensePickerWidget *>();
	QVERIFY(picker != nullptr);
	QCOMPARE(static_cast<int>(picker->license().kind), static_cast<int>(LicenseKind::Custom));
	const RedistributionRule result = picker->redistributionRule();
	QVERIFY(result.allowed);
	QVERIFY(result.attributionRequired);
}

void TestDuplicateLibraryDialog::validationBlocksSameValuesAsSource() {
	const Library source = makeSource(LicenseKind::MIT, redistributionRuleFor(LicenseKind::MIT));
	DuplicateLibraryDialog dialog(source);

	// 複製元と全く同じ値に戻すと、accept は失敗するはず。フィールドへの直接アクセスは
	// private なので、オブジェクト名を振った QLineEdit を名前引きして値を設定する。
	dialog.findChild<QLineEdit *>(QStringLiteral("idEdit"))->setText(source.id);
	dialog.findChild<QLineEdit *>(QStringLiteral("nameEdit"))->setText(source.name);
	dialog.findChild<QLineEdit *>(QStringLiteral("authorEdit"))->setText(source.author);
	dialog.findChild<QLineEdit *>(QStringLiteral("versionEdit"))->setText(source.version);

	// onAccept() は検証に失敗すると QMessageBox::warning (モーダル、内部で exec()) を
	// 出す。テストでは誰もクリックしないと無限に止まってしまうので、ネストした
	// イベントループに入ったところを狙って自動的に閉じる。
	QTimer::singleShot(0, [] {
		if (QWidget *modal = QApplication::activeModalWidget()) {
			modal->close();
		}
	});
	QMetaObject::invokeMethod(&dialog, "onAccept", Qt::DirectConnection);
	QVERIFY(dialog.result() != QDialog::Accepted);
}

void TestDuplicateLibraryDialog::validationPassesWhenAllFieldsDiffer() {
	const Library source = makeSource(LicenseKind::MIT, redistributionRuleFor(LicenseKind::MIT));
	DuplicateLibraryDialog dialog(source);

	dialog.findChild<QLineEdit *>(QStringLiteral("idEdit"))->setText(QStringLiteral("different-id"));
	dialog.findChild<QLineEdit *>(QStringLiteral("nameEdit"))->setText(QStringLiteral("違う名前"));
	dialog.findChild<QLineEdit *>(QStringLiteral("authorEdit"))->setText(QStringLiteral("違う作者"));
	dialog.findChild<QLineEdit *>(QStringLiteral("versionEdit"))->setText(QStringLiteral("2.0.0"));

	QMetaObject::invokeMethod(&dialog, "onAccept", Qt::DirectConnection);
	QCOMPARE(dialog.result(), static_cast<int>(QDialog::Accepted));

	const auto spec = dialog.spec();
	QCOMPARE(spec.newId, QStringLiteral("different-id"));
	QCOMPARE(spec.newAuthor, QStringLiteral("違う作者"));
	QCOMPARE(spec.newVersion, QStringLiteral("2.0.0"));
}

QTEST_MAIN(TestDuplicateLibraryDialog)
#include "test_duplicatelibrarydialog.moc"
