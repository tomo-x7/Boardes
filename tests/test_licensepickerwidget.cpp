#include <QTest>

#include "ui/licensepickerwidget.h"

class TestLicensePickerWidget : public QObject {
	Q_OBJECT

private slots:
	void roundTripsSimpleKind();
	void roundTripsCustomFields();
	void allowedKindsRestrictsSelection();
	void allowedKindsFallsBackToFirstWhenCurrentNotAllowed();
};

void TestLicensePickerWidget::roundTripsSimpleKind() {
	LicensePickerWidget picker;
	LicenseInfo info;
	info.kind = LicenseKind::MIT;
	picker.setLicense(info);

	const LicenseInfo result = picker.license();
	QCOMPARE(static_cast<int>(result.kind), static_cast<int>(LicenseKind::MIT));
}

void TestLicensePickerWidget::roundTripsCustomFields() {
	LicensePickerWidget picker;
	LicenseInfo info;
	info.kind = LicenseKind::Custom;
	info.customName = QStringLiteral("独自ライセンス");
	info.customUrl = QStringLiteral("https://example.com/license");
	info.customLicenseText = QStringLiteral("全文...");
	picker.setLicense(info);

	const LicenseInfo result = picker.license();
	QCOMPARE(static_cast<int>(result.kind), static_cast<int>(LicenseKind::Custom));
	QCOMPARE(result.customName, info.customName);
	QCOMPARE(result.customUrl, info.customUrl);
	QCOMPARE(result.customLicenseText, info.customLicenseText);
}

void TestLicensePickerWidget::allowedKindsRestrictsSelection() {
	LicensePickerWidget picker;
	LicenseInfo info;
	info.kind = LicenseKind::CC_BY_SA_4_0;
	picker.setLicense(info);
	// CC-BY-SA-4.0 だけに制限しても、現在の選択がそのリストに含まれているので維持される。
	picker.setAllowedKinds({LicenseKind::CC_BY_SA_4_0});
	QCOMPARE(static_cast<int>(picker.license().kind), static_cast<int>(LicenseKind::CC_BY_SA_4_0));
}

void TestLicensePickerWidget::allowedKindsFallsBackToFirstWhenCurrentNotAllowed() {
	LicensePickerWidget picker;
	LicenseInfo info;
	info.kind = LicenseKind::MIT;
	picker.setLicense(info);
	// MIT は許可リストに含まれないので、リストの先頭 (CC_BY_NC_4_0) にフォールバックする。
	picker.setAllowedKinds({LicenseKind::CC_BY_NC_4_0, LicenseKind::Custom});
	QCOMPARE(static_cast<int>(picker.license().kind), static_cast<int>(LicenseKind::CC_BY_NC_4_0));
}

QTEST_MAIN(TestLicensePickerWidget)
#include "test_licensepickerwidget.moc"
