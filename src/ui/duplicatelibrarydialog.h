#pragma once

#include <QDialog>

#include "../model/librarymanager.h"

class QLineEdit;
class QLabel;
class LicensePickerWidget;

// ライブラリの複製ダイアログ (PasS 互換など読み込み専用のライブラリを編集可能にする
// 唯一の経路)。id/name/author/version は複製元と異なる値への変更を強制する。
// 元ライセンスの派生ポリシーがコピーレフト系/NC系固定であれば、ライセンス選択も
// それに応じて制限する。
class DuplicateLibraryDialog : public QDialog {
	Q_OBJECT

public:
	DuplicateLibraryDialog(const Library &source, QWidget *parent = nullptr);

	// OK で閉じた後に呼ぶ。LibraryManager::duplicateLibrary にそのまま渡せる。
	LibraryManager::DuplicateSpec spec() const;

private slots:
	void onAccept();

private:
	Library m_source;
	QLineEdit *m_idEdit;
	QLineEdit *m_nameEdit;
	QLineEdit *m_authorEdit;
	QLineEdit *m_versionEdit;
	LicensePickerWidget *m_licensePicker;
};
