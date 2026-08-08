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
	// 派生ライセンスを元のライセンスに強制する仕組み (コピーレフト強制) は廃止した
	// (Phase 14)。複製物のライセンスは常に自由に選べる — 尊重すべきは「元の再配布可否」
	// までであり、複製物自身の公開・再配布可否は複製物自身のライセンスで決まる。
	m_licensePicker->setLicense(source.license);
	m_licensePicker->setRedistributionRule(source.redistribution);

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
	s.newRedistribution = m_licensePicker->redistributionRule();
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
