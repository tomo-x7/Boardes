#pragma once

#include <QDialog>

class LibraryManager;
class QListWidget;
class QLabel;
class QPushButton;

// ライブラリの一覧・詳細表示・インストール(.blib)/複製/エクスポート(.blib)/削除/
// メタデータ編集をまとめて行うダイアログ。メニュー「ライブラリ→ライブラリ管理...」
// から開く。部品/基板ファイル単体のインポートや新規作成は、ここではなく
// ファイル→インポートおよびライブラリ→新しい基板/部品を作成、から行う
// (このダイアログは「既存のライブラリそのもの」のライフサイクル管理に専念する)。
class LibraryManagerDialog : public QDialog {
	Q_OBJECT

public:
	explicit LibraryManagerDialog(LibraryManager *libraryManager, QWidget *parent = nullptr);

private slots:
	void onSelectionChanged();
	void onInstallBlib();
	void onDuplicate();
	void onExport();
	void onRemove();
	void onEditMetadata();
	void refreshList();

private:
	LibraryManager *m_libraryManager;
	QListWidget *m_list;
	QLabel *m_detailsLabel;
	QPushButton *m_duplicateButton;
	QPushButton *m_exportButton;
	QPushButton *m_removeButton;
	QPushButton *m_editMetadataButton;

	QString selectedLibraryId() const;
	void updateButtonStates();
};
