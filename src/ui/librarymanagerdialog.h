#pragma once

#include <QDialog>

#include "../model/board.h"

class LibraryManager;
class QTreeWidget;
class QTreeWidgetItem;
class QLabel;
class QPushButton;

// ライブラリのブラウズ・編集ハブ。メニュー「ライブラリ→ライブラリ管理...」から開く
// 唯一の入口 (Phase 14 — ファイル→インポートや個別の新規作成メニューは廃止し、
// すべての取込・作成・編集・削除操作をここに集約した)。
//
// 左側はツリー (ライブラリ > カテゴリ/基板 > 部品/基板)、右側は詳細表示と操作ボタン群。
// Phase 14 で readOnly の区別を廃止したため、どのライブラリも編集・削除できる。
class LibraryManagerDialog : public QDialog {
	Q_OBJECT

public:
	explicit LibraryManagerDialog(LibraryManager *libraryManager, QWidget *parent = nullptr);

	// ツリーのノード種別。setNodeData() ヘルパ (.cpp 内の無名名前空間) からも使うため公開。
	enum class NodeType { Root, Library, CategoryRoot, Category, BoardRoot, Part, Board };

signals:
	// 基板ノードで「現在のデザインに使用」を選んだとき。MainWindow が Undo コマンド
	// 経由で適用する (このダイアログが Document を直接触らないようにするため)。
	void boardApplyRequested(const BoardSpec &board);

private slots:
	void onSelectionChanged();
	void refreshTree();

	void onNewLibrary();
	void onDuplicate();
	void onExport();
	void onRemoveLibrary();
	void onEditMetadata();

	void onInstallBlib();
	void onImportPassFolder();
	void onImportPartFile();
	void onImportBoardFile();

	void onAddPart();
	void onAddBoard();
	void onCopyFromOtherLibrary();
	void onEditSelected();
	void onDeleteSelectedItem();
	void onChangeCategory();
	void onUseSelectedBoard();

	void onAddCategory();
	void onRenameCategory();
	void onDeleteCategory();

private:
	LibraryManager *m_libraryManager;
	QTreeWidget *m_tree;
	QLabel *m_detailsLabel;

	QPushButton *m_newLibraryButton;
	QPushButton *m_duplicateButton;
	QPushButton *m_removeButton;
	QPushButton *m_editMetadataButton;
	QPushButton *m_exportButton;

	QPushButton *m_addPartButton;
	QPushButton *m_addBoardButton;
	QPushButton *m_copyFromOtherButton;
	QPushButton *m_editSelectedButton;
	QPushButton *m_deleteSelectedButton;
	QPushButton *m_changeCategoryButton;
	QPushButton *m_useBoardButton;

	QPushButton *m_addCategoryButton;
	QPushButton *m_renameCategoryButton;
	QPushButton *m_deleteCategoryButton;

	struct Selection {
		NodeType type = NodeType::Root;
		QString libId;
		QString itemId;  // partId / boardId / categoryId (type に応じる)
	};
	Selection currentSelection() const;
	// 選択位置から「今操作対象にすべきライブラリ id」を導出する (ライブラリ配下の
	// どのノードを選んでいても、追加系ボタンが迷わず使えるようにするため)。
	QString contextLibraryId() const;
	// 部品を追加する先のカテゴリ id (Category ノードが選ばれていればそれ、それ以外は空
	// = 未分類)。
	QString contextCategoryId() const;

	void updateButtonStates();
	void updateDetails();
	bool confirmRedistributionWarning(const QString &libId);
};
