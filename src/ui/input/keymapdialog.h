#pragma once

#include <QDialog>

class Keymap;
class QListWidget;
class QTreeWidget;
class QTreeWidgetItem;
class QLabel;
class QPushButton;

// 「操作のカスタマイズ」ダイアログ。キーボード/マウスの割り当てをコマンド単位で
// 追加・削除・既定に戻すことができる (Phase 18)。カテゴリ内 (+ global との) 重複は
// 警告表示するのみで、保存はブロックしない。
class KeymapDialog : public QDialog {
	Q_OBJECT

public:
	explicit KeymapDialog(Keymap *keymap, QWidget *parent = nullptr);

private slots:
	void onCategorySelectionChanged();
	void onCommandSelectionChanged();
	void onGestureSelectionChanged();
	void onAddGesture();
	void onRemoveGesture();
	void onResetCommand();
	void onResetAll();
	void onExportJson();
	void onImportJson();

private:
	Keymap *m_keymap;
	QListWidget *m_categoryList = nullptr;
	QTreeWidget *m_commandTree = nullptr;   // 選択中カテゴリのコマンド一覧
	QTreeWidget *m_gestureTree = nullptr;   // 選択中コマンドに割り当てられているジェスチャー一覧
	QLabel *m_conflictLabel = nullptr;
	QPushButton *m_removeButton = nullptr;
	QPushButton *m_resetButton = nullptr;

	void rebuildCategoryList();
	void rebuildCommandTree();
	void rebuildGestureTree();
	void updateConflictLabel();
	QString currentCommandId() const;
};
