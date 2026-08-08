#pragma once

#include <QDialog>

#include "../model/librarymanager.h"

class QLineEdit;
class QLabel;
class LicensePickerWidget;

// ライブラリの複製ダイアログ。id/name/author/version は複製元と異なる値への変更を
// 強制する。Phase 14 で全ライブラリが直接編集可能になったため、このダイアログの
// 役割は「独立したコピーを作る」ことに絞られる (派生ライセンスを元に強制する仕組みは
// 廃止した — 複製物のライセンスは常に自由に選べる)。
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
