#include "librarymetadatadialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QTextEdit>
#include <QVBoxLayout>

#include "licensepickerwidget.h"

LibraryMetadataDialog::LibraryMetadataDialog(QWidget *parent) : QDialog(parent) {
	setWindowTitle(tr("ライブラリのメタデータを編集"));

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
	form->addRow(tr("名前:"), m_nameEdit);
	form->addRow(tr("バージョン:"), m_versionEdit);
	form->addRow(tr("作者:"), m_authorEdit);
	form->addRow(tr("作者URL:"), m_authorUrlEdit);
	form->addRow(tr("ホームページ:"), m_homepageEdit);
	form->addRow(tr("説明:"), m_descriptionEdit);
	form->addRow(tr("ライセンス:"), m_licensePicker);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(buttons, &QDialogButtonBox::accepted, this, &LibraryMetadataDialog::onAccept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	auto *layout = new QVBoxLayout(this);
	layout->addLayout(form);
	layout->addWidget(buttons);
	resize(480, 520);
}

void LibraryMetadataDialog::setLibrary(const Library &lib) {
	m_nameEdit->setText(lib.name);
	m_versionEdit->setText(lib.version);
	m_authorEdit->setText(lib.author);
	m_authorUrlEdit->setText(lib.authorUrl);
	m_homepageEdit->setText(lib.homepage);
	m_descriptionEdit->setPlainText(lib.description);
	m_licensePicker->setLicense(lib.license);
}

void LibraryMetadataDialog::applyTo(Library &lib) const {
	lib.name = m_nameEdit->text().trimmed();
	lib.version = m_versionEdit->text().trimmed();
	lib.author = m_authorEdit->text().trimmed();
	lib.authorUrl = m_authorUrlEdit->text().trimmed();
	lib.homepage = m_homepageEdit->text().trimmed();
	lib.description = m_descriptionEdit->toPlainText();
	lib.license = m_licensePicker->license();
	lib.redistribution = redistributionRuleFor(lib.license.kind);
}

void LibraryMetadataDialog::onAccept() {
	if (m_nameEdit->text().trimmed().isEmpty()) {
		QMessageBox::warning(this, tr("入力エラー"), tr("名前は必須です。"));
		return;
	}
	accept();
}
