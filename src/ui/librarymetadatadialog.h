#pragma once

#include <QDialog>

#include "../model/library.h"

class QLineEdit;
class QTextEdit;
class QDialogButtonBox;
class LicensePickerWidget;

// ライブラリのメタデータを編集するダイアログ。Phase 14 で全ライブラリが編集可能に
// なったため、新規作成 (id も入力する) にも既存編集 (id は固定) にも使う。
// 部品/基板/カテゴリの中身はここでは扱わないため対象外。
class LibraryMetadataDialog : public QDialog {
	Q_OBJECT

public:
	enum class Mode { Create, Edit };

	explicit LibraryMetadataDialog(QWidget *parent = nullptr);

	// タイトル・OK ボタン文言・id 欄の編集可否を切り替える。既定は Create。
	void setMode(Mode mode);

	void setLibrary(const Library &lib);
	// name/version/author/authorUrl/homepage/description/license (+そこから再導出した
	// redistribution) のみを反映する。parts/boards/categories/basedOn は呼び出し側が
	// 保持している値のまま変更しない。Create モードのときは id も反映する
	// (Edit モードでは呼び出し側が id を上書きしないよう注意すること)。
	void applyTo(Library &lib) const;
	// Create モードでのみ意味を持つ、入力された id (trim 済み)。
	QString enteredId() const;

private slots:
	void onAccept();

private:
	Mode m_mode = Mode::Create;
	QLineEdit *m_idEdit;
	QLineEdit *m_nameEdit;
	QLineEdit *m_versionEdit;
	QLineEdit *m_authorEdit;
	QLineEdit *m_authorUrlEdit;
	QLineEdit *m_homepageEdit;
	QTextEdit *m_descriptionEdit;
	LicensePickerWidget *m_licensePicker;
	QDialogButtonBox *m_buttons = nullptr;
};
