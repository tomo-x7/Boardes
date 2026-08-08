#pragma once

#include <QDialog>
#include <QVector>

#include "toolbarlayout.h"

class ActionRegistry;
class QComboBox;
class QListWidget;
class QListWidgetItem;
class QCheckBox;
class QLabel;
class QPushButton;

// 「ツールバーのカスタマイズ」ダイアログ (LibreOffice 風。Phase 19)。
// メニューバー自体はカスタマイズ対象外 (LibreOffice に倣う)。
//
// 安全のため、既存の「表示」「ツール」ツールバー (builtin) は表示/非表示と
// 表示スタイルのみ変更でき、項目の追加・削除・並べ替えはできない
// (これらのツールバーは特別なウィジェット (コンボボックス等) を含んでおり、
// 一般的な commandId の並びだけでは安全に組み立て直せないため)。
// 項目を自由に選んで並べたい場合は「新しいツールバー」を作る。
class ToolbarCustomizeDialog : public QDialog {
	Q_OBJECT

public:
	explicit ToolbarCustomizeDialog(ActionRegistry *registry, QWidget *parent = nullptr);

signals:
	// 保存内容が変わるたびに発火する。MainWindow はこれを受けて rebuildToolbars() を呼ぶ。
	void layoutsChanged();

private slots:
	void onToolbarComboChanged(int index);
	void onNewToolbar();
	void onRenameToolbar();
	void onDeleteToolbar();
	void onAddItem();
	void onRemoveItem();
	void onMoveUp();
	void onMoveDown();
	void onAddSeparator();
	void onStyleChanged(int index);
	void onVisibleToggled(bool checked);
	void onResetAll();

private:
	ActionRegistry *m_registry;
	QVector<ToolbarLayout> m_layouts;  // 編集中の作業コピー (変更のたびに即保存する)

	QComboBox *m_toolbarCombo = nullptr;
	QPushButton *m_renameButton = nullptr;
	QPushButton *m_deleteButton = nullptr;
	QListWidget *m_availableList = nullptr;
	QListWidget *m_itemsList = nullptr;
	QPushButton *m_addButton = nullptr;
	QPushButton *m_removeButton = nullptr;
	QPushButton *m_upButton = nullptr;
	QPushButton *m_downButton = nullptr;
	QPushButton *m_separatorButton = nullptr;
	QComboBox *m_styleCombo = nullptr;
	QCheckBox *m_visibleCheck = nullptr;
	QLabel *m_builtinNoticeLabel = nullptr;

	ToolbarLayout *currentLayout();
	QString labelFor(const QString &commandId) const;

	void rebuildToolbarCombo();
	void rebuildAvailableAndItemsLists();
	void updateEditableState();
	void persistAndNotify();
};
