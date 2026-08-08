#include "librarymetadatadialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

#include "licensepickerwidget.h"

LibraryMetadataDialog::LibraryMetadataDialog(QWidget *parent) : QDialog(parent) {
	m_idEdit = new QLineEdit(this);
	m_idEdit->setPlaceholderText(tr("例: com.example.my-parts"));
	m_nameEdit = new QLineEdit(this);
	m_versionEdit = new QLineEdit(this);
	m_versionEdit->setPlaceholderText(tr("例: 1.0.0"));
	m_authorEdit = new QLineEdit(this);
	m_authorUrlEdit = new QLineEdit(this);
	m_authorUrlEdit->setPlaceholderText(tr("https://..."));
	m_homepageEdit = new QLineEdit(this);
	m_homepageEdit->setPlaceholderText(tr("https://..."));
	m_descriptionEdit = new QTextEdit(this);
	m_descriptionEdit->setMaximumHeight(80);

	m_licensePicker = new LicensePickerWidget(this);

	auto *form = new QFormLayout();
	form->addRow(tr("ID:"), m_idEdit);
	form->addRow(tr("名前:"), m_nameEdit);
	form->addRow(tr("バージョン:"), m_versionEdit);
	form->addRow(tr("作者:"), m_authorEdit);
	form->addRow(tr("作者URL:"), m_authorUrlEdit);
	form->addRow(tr("ホームページ:"), m_homepageEdit);
	form->addRow(tr("説明:"), m_descriptionEdit);
	form->addRow(tr("ライセンス:"), m_licensePicker);

	m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(m_buttons, &QDialogButtonBox::accepted, this, &LibraryMetadataDialog::onAccept);
	connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	auto *layout = new QVBoxLayout(this);
	layout->addLayout(form);
	layout->addWidget(m_buttons);
	resize(480, 560);

	setMode(Mode::Create);
}

void LibraryMetadataDialog::setMode(Mode mode) {
	m_mode = mode;
	if (mode == Mode::Create) {
		setWindowTitle(tr("ライブラリを作成"));
		m_buttons->button(QDialogButtonBox::Ok)->setText(tr("作成"));
		m_idEdit->setEnabled(true);
	} else {
		setWindowTitle(tr("ライブラリのメタデータを編集"));
		m_buttons->button(QDialogButtonBox::Ok)->setText(tr("保存"));
		m_idEdit->setEnabled(false);
	}
}

void LibraryMetadataDialog::setLibrary(const Library &lib) {
	m_idEdit->setText(lib.id);
	m_nameEdit->setText(lib.name);
	m_versionEdit->setText(lib.version);
	m_authorEdit->setText(lib.author);
	m_authorUrlEdit->setText(lib.authorUrl);
	m_homepageEdit->setText(lib.homepage);
	m_descriptionEdit->setPlainText(lib.description);
	m_licensePicker->setLicense(lib.license);
	m_licensePicker->setRedistributionRule(lib.redistribution);
	setMode(Mode::Edit);
}

QString LibraryMetadataDialog::enteredId() const {
	return m_idEdit->text().trimmed();
}

void LibraryMetadataDialog::applyTo(Library &lib) const {
	if (m_mode == Mode::Create) {
		lib.id = enteredId();
	}
	lib.name = m_nameEdit->text().trimmed();
	lib.version = m_versionEdit->text().trimmed();
	lib.author = m_authorEdit->text().trimmed();
	lib.authorUrl = m_authorUrlEdit->text().trimmed();
	lib.homepage = m_homepageEdit->text().trimmed();
	lib.description = m_descriptionEdit->toPlainText();
	lib.license = m_licensePicker->license();
	lib.redistribution = m_licensePicker->redistributionRule();
}

void LibraryMetadataDialog::onAccept() {
	if (m_nameEdit->text().trimmed().isEmpty()) {
		QMessageBox::warning(this, tr("入力エラー"), tr("名前は必須です。"));
		return;
	}
	if (m_mode == Mode::Create && enteredId().isEmpty()) {
		QMessageBox::warning(this, tr("入力エラー"), tr("ID は必須です。"));
		return;
	}
	accept();
}
