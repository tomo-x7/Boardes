#include "licensepickerwidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QVBoxLayout>

namespace {

const QVector<LicenseKind> &allLicenseKinds() {
	static const QVector<LicenseKind> kinds = {
		LicenseKind::CC0_1_0, LicenseKind::CC_BY_4_0, LicenseKind::MIT,
		LicenseKind::AllRightsReserved, LicenseKind::Custom,
	};
	return kinds;
}

}  // namespace

LicensePickerWidget::LicensePickerWidget(QWidget *parent) : QWidget(parent) {
	m_kindCombo = new QComboBox(this);

	m_customNameEdit = new QLineEdit(this);
	m_customNameEdit->setPlaceholderText(tr("例: 独自ライセンス v1"));
	m_customUrlEdit = new QLineEdit(this);
	m_customUrlEdit->setPlaceholderText(tr("ライセンス全文へのURL (任意)"));
	m_customTextEdit = new QTextEdit(this);
	m_customTextEdit->setPlaceholderText(tr("ライセンス全文 (任意。入力するとパッケージに LICENSE.txt として同梱されます)"));
	m_customTextEdit->setMaximumHeight(80);
	m_customAllowedCheck = new QCheckBox(tr("再配布を許可する"), this);
	m_customAttributionCheck = new QCheckBox(tr("著作権表示を必須にする"), this);

	m_customFieldsWidget = new QWidget(this);
	auto *customForm = new QFormLayout(m_customFieldsWidget);
	customForm->setContentsMargins(0, 0, 0, 0);
	customForm->addRow(tr("名前:"), m_customNameEdit);
	customForm->addRow(tr("URL:"), m_customUrlEdit);
	customForm->addRow(tr("全文:"), m_customTextEdit);
	customForm->addRow(QString(), m_customAllowedCheck);
	customForm->addRow(QString(), m_customAttributionCheck);

	m_previewLabel = new QLabel(this);
	m_previewLabel->setWordWrap(true);
	m_previewLabel->setStyleSheet(QStringLiteral("color: palette(mid);"));

	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(m_kindCombo);
	layout->addWidget(m_customFieldsWidget);
	layout->addWidget(m_previewLabel);

	rebuildComboItems();
	connect(m_kindCombo, &QComboBox::currentIndexChanged, this, &LicensePickerWidget::onKindComboChanged);
	connect(m_customNameEdit, &QLineEdit::textChanged, this, &LicensePickerWidget::licenseChanged);
	connect(m_customUrlEdit, &QLineEdit::textChanged, this, &LicensePickerWidget::licenseChanged);
	connect(m_customTextEdit, &QTextEdit::textChanged, this, &LicensePickerWidget::licenseChanged);
	connect(m_customAllowedCheck, &QCheckBox::toggled, this, &LicensePickerWidget::updatePreview);
	connect(m_customAttributionCheck, &QCheckBox::toggled, this, &LicensePickerWidget::updatePreview);

	updateCustomFieldsVisibility();
	updatePreview();
}

void LicensePickerWidget::rebuildComboItems() {
	const QVector<LicenseKind> &list = m_allowedKinds.isEmpty() ? allLicenseKinds() : m_allowedKinds;
	m_kindCombo->blockSignals(true);
	m_kindCombo->clear();
	for (LicenseKind k : list) {
		m_kindCombo->addItem(k == LicenseKind::Custom ? tr("カスタム...") : licenseDisplayName(k),
							  static_cast<int>(k));
	}
	m_kindCombo->blockSignals(false);
}

void LicensePickerWidget::setAllowedKinds(const QVector<LicenseKind> &kinds) {
	m_allowedKinds = kinds;
	const int currentKind = m_kindCombo->currentData().toInt();
	rebuildComboItems();
	const int idx = m_kindCombo->findData(currentKind);
	m_kindCombo->setCurrentIndex(idx >= 0 ? idx : 0);
	updateCustomFieldsVisibility();
	updatePreview();
}

void LicensePickerWidget::setLicense(const LicenseInfo &info) {
	const int idx = m_kindCombo->findData(static_cast<int>(info.kind));
	m_kindCombo->setCurrentIndex(idx >= 0 ? idx : 0);
	m_customNameEdit->setText(info.customName);
	m_customUrlEdit->setText(info.customUrl);
	m_customTextEdit->setPlainText(info.customLicenseText);
	updateCustomFieldsVisibility();
	updatePreview();
}

LicenseInfo LicensePickerWidget::license() const {
	LicenseInfo info;
	info.kind = static_cast<LicenseKind>(m_kindCombo->currentData().toInt());
	info.customName = m_customNameEdit->text().trimmed();
	info.customUrl = m_customUrlEdit->text().trimmed();
	info.customLicenseText = m_customTextEdit->toPlainText();
	return info;
}

void LicensePickerWidget::setRedistributionRule(const RedistributionRule &rule) {
	m_customAllowedCheck->setChecked(rule.allowed);
	m_customAttributionCheck->setChecked(rule.attributionRequired);
	updatePreview();
}

RedistributionRule LicensePickerWidget::redistributionRule() const {
	const LicenseKind kind = static_cast<LicenseKind>(m_kindCombo->currentData().toInt());
	if (kind != LicenseKind::Custom) {
		return redistributionRuleFor(kind);
	}
	RedistributionRule rule;
	rule.allowed = m_customAllowedCheck->isChecked();
	rule.attributionRequired = m_customAttributionCheck->isChecked();
	return rule;
}

void LicensePickerWidget::onKindComboChanged(int) {
	updateCustomFieldsVisibility();
	updatePreview();
}

void LicensePickerWidget::updateCustomFieldsVisibility() {
	const LicenseKind kind = static_cast<LicenseKind>(m_kindCombo->currentData().toInt());
	m_customFieldsWidget->setVisible(kind == LicenseKind::Custom);
}

void LicensePickerWidget::updatePreview() {
	const RedistributionRule rule = redistributionRule();
	QString text;
	if (!rule.allowed) {
		text = tr("再配布: 不可 (このライブラリを他人と共有する用途にはエクスポートできません)");
	} else {
		text = tr("再配布: 可%1").arg(rule.attributionRequired ? tr(" (著作権表示が必要)") : QString());
	}
	m_previewLabel->setText(text);
	emit licenseChanged();
}
