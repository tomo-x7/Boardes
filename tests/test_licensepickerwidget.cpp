#include <QTest>

#include "ui/licensepickerwidget.h"

class TestLicensePickerWidget : public QObject {
	Q_OBJECT

private slots:
	void roundTripsSimpleKind();
	void roundTripsCustomFields();
	void allowedKindsRestrictsSelection();
	void allowedKindsFallsBackToFirstWhenCurrentNotAllowed();
	void customKindDerivesRedistributionFromCheckboxes();
	void nonCustomKindIgnoresManualRedistribution();
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
	info.kind = LicenseKind::CC0_1_0;
	picker.setLicense(info);
	// CC0-1.0 だけに制限しても、現在の選択がそのリストに含まれているので維持される。
	picker.setAllowedKinds({LicenseKind::CC0_1_0});
	QCOMPARE(static_cast<int>(picker.license().kind), static_cast<int>(LicenseKind::CC0_1_0));
}

void TestLicensePickerWidget::allowedKindsFallsBackToFirstWhenCurrentNotAllowed() {
	LicensePickerWidget picker;
	LicenseInfo info;
	info.kind = LicenseKind::MIT;
	picker.setLicense(info);
	// MIT は許可リストに含まれないので、リストの先頭 (AllRightsReserved) にフォールバックする。
	picker.setAllowedKinds({LicenseKind::AllRightsReserved, LicenseKind::Custom});
	QCOMPARE(static_cast<int>(picker.license().kind), static_cast<int>(LicenseKind::AllRightsReserved));
}

void TestLicensePickerWidget::customKindDerivesRedistributionFromCheckboxes() {
	LicensePickerWidget picker;
	LicenseInfo info;
	info.kind = LicenseKind::Custom;
	picker.setLicense(info);
	RedistributionRule rule;
	rule.allowed = true;
	rule.attributionRequired = true;
	picker.setRedistributionRule(rule);

	const RedistributionRule result = picker.redistributionRule();
	QVERIFY(result.allowed);
	QVERIFY(result.attributionRequired);
}

void TestLicensePickerWidget::nonCustomKindIgnoresManualRedistribution() {
	LicensePickerWidget picker;
	LicenseInfo info;
	info.kind = LicenseKind::CC0_1_0;
	picker.setLicense(info);
	// Custom 以外は redistributionRuleFor() の固定値を使う。チェックボックスの状態には
	// 左右されない (CC0 は表示不要・再配布可のはず)。
	picker.setRedistributionRule(RedistributionRule{});  // allowed=false, attributionRequired=false
	const RedistributionRule result = picker.redistributionRule();
	QVERIFY(result.allowed);
	QVERIFY(!result.attributionRequired);
}

QTEST_MAIN(TestLicensePickerWidget)
#include "test_licensepickerwidget.moc"
