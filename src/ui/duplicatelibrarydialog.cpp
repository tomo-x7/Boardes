#include "duplicatelibrarydialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QVBoxLayout>

#include "licensepickerwidget.h"

DuplicateLibraryDialog::DuplicateLibraryDialog(const Library &source, QWidget *parent)
	: QDialog(parent), m_source(source) {
	setWindowTitle(tr("ライブラリを複製"));

	auto *basedOnLabel = new QLabel(
		tr("複製元: %1 (v%2) — %3\nid/名前/作者/バージョンはすべて複製元と異なる値に変更してください。")
			.arg(source.name, source.version, source.license.displayName()),
		this);
	basedOnLabel->setWordWrap(true);

	// findChild<QLineEdit*>(name) で個別に引けるよう、オブジェクト名を振っておく
	// (テストや将来の拡張で「特定のフィールドだけ」を指し示したいときのため)。
	m_idEdit = new QLineEdit(this);
	m_idEdit->setObjectName(QStringLiteral("idEdit"));
	m_idEdit->setText(source.id + QStringLiteral("-copy"));
	m_nameEdit = new QLineEdit(this);
	m_nameEdit->setObjectName(QStringLiteral("nameEdit"));
	m_nameEdit->setText(source.name + tr(" (複製)"));
	m_authorEdit = new QLineEdit(this);
	m_authorEdit->setObjectName(QStringLiteral("authorEdit"));
	m_authorEdit->setPlaceholderText(tr("あなたの名前"));
	m_versionEdit = new QLineEdit(this);
	m_versionEdit->setObjectName(QStringLiteral("versionEdit"));
	m_versionEdit->setText(QStringLiteral("1.0.0"));

	m_licensePicker = new LicensePickerWidget(this);
	const RedistributionRule &rule = source.redistribution;
	if (rule.allowed && rule.derivativePolicy == DerivativePolicy::MustMatchSame) {
		m_licensePicker->setAllowedKinds({source.license.kind});
	} else if (rule.allowed && rule.derivativePolicy == DerivativePolicy::NcFamilyOnly) {
		m_licensePicker->setAllowedKinds({LicenseKind::CC_BY_NC_4_0, LicenseKind::Custom});
	}
	// rule.allowed==false (PasS互換など再配布不可の元) の場合は制限しない。複製物は
	// 独立した創作物として利用者の責任で新しくライセンスを設定できる
	// (LibraryManager::duplicateLibrary の既存仕様通り)。
	m_licensePicker->setLicense(source.license);

	auto *form = new QFormLayout();
	form->addRow(tr("ID:"), m_idEdit);
	form->addRow(tr("名前:"), m_nameEdit);
	form->addRow(tr("作者:"), m_authorEdit);
	form->addRow(tr("バージョン:"), m_versionEdit);
	form->addRow(tr("ライセンス:"), m_licensePicker);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(buttons, &QDialogButtonBox::accepted, this, &DuplicateLibraryDialog::onAccept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	auto *layout = new QVBoxLayout(this);
	layout->addWidget(basedOnLabel);
	layout->addLayout(form);
	layout->addWidget(buttons);
	resize(480, 480);
}

LibraryManager::DuplicateSpec DuplicateLibraryDialog::spec() const {
	LibraryManager::DuplicateSpec s;
	s.newId = m_idEdit->text().trimmed();
	s.newName = m_nameEdit->text().trimmed();
	s.newAuthor = m_authorEdit->text().trimmed();
	s.newVersion = m_versionEdit->text().trimmed();
	s.newLicense = m_licensePicker->license();
	return s;
}

void DuplicateLibraryDialog::onAccept() {
	const QString id = m_idEdit->text().trimmed();
	const QString name = m_nameEdit->text().trimmed();
	const QString author = m_authorEdit->text().trimmed();
	const QString version = m_versionEdit->text().trimmed();

	if (id.isEmpty() || name.isEmpty() || author.isEmpty() || version.isEmpty()) {
		QMessageBox::warning(this, tr("入力エラー"), tr("ID・名前・作者・バージョンはすべて必須です。"));
		return;
	}
	if (id == m_source.id) {
		QMessageBox::warning(this, tr("入力エラー"), tr("ID は複製元と異なる値にしてください。"));
		return;
	}
	if (name == m_source.name) {
		QMessageBox::warning(this, tr("入力エラー"), tr("名前は複製元と異なる値にしてください。"));
		return;
	}
	if (author == m_source.author) {
		QMessageBox::warning(this, tr("入力エラー"), tr("作者は複製元と異なる値にしてください。"));
		return;
	}
	if (version == m_source.version) {
		QMessageBox::warning(this, tr("入力エラー"), tr("バージョンは複製元と異なる値にしてください。"));
		return;
	}
	accept();
}
