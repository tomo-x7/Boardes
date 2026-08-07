#pragma once

#include <QDialog>

#include "../model/library.h"

class QLineEdit;
class QTextEdit;
class LicensePickerWidget;

// 編集可能なライブラリ (readOnly==false、通常はマイライブラリか複製で作ったもの) の
// メタデータを編集するダイアログ。id はライブラリの同一性そのもの (Placement からの
// 参照キー) であり、部品/基板/カテゴリの中身もここでは扱わないため対象外とする。
class LibraryMetadataDialog : public QDialog {
	Q_OBJECT

public:
	explicit LibraryMetadataDialog(QWidget *parent = nullptr);

	void setLibrary(const Library &lib);
	// name/version/author/authorUrl/homepage/description/license (+そこから再導出した
	// redistribution) のみを反映する。id/parts/boards/categories/basedOn/readOnly は
	// 呼び出し側が保持している値のまま変更しない。
	void applyTo(Library &lib) const;

private slots:
	void onAccept();

private:
	QLineEdit *m_nameEdit;
	QLineEdit *m_versionEdit;
	QLineEdit *m_authorEdit;
	QLineEdit *m_authorUrlEdit;
	QLineEdit *m_homepageEdit;
	QTextEdit *m_descriptionEdit;
	LicensePickerWidget *m_licensePicker;
};
