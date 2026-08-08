#pragma once

#include <QWidget>

struct ToolContext;
class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;

// 右ドックの「オブジェクト」タブ。現在の設計データに含まれる部品・配線を一覧表示し、
// そこからの選択・表示切替・簡単な編集操作をまとめて行える (Office の「オブジェクト」
// 一覧に相当)。ToolContext を直接持つ (Tool 派生クラスと同じ考え方) ので、Undo
// コマンドを自分で push できる。
class ObjectListPanel : public QWidget {
	Q_OBJECT

public:
	explicit ObjectListPanel(QWidget *parent = nullptr);

	// 非所有。nullptr にすると空表示になる。ドキュメント切替のたびに呼び直すこと。
	void setContext(ToolContext *ctx);

public slots:
	// ドキュメントの内容が変わるたびに呼ぶ (undo/redo/編集コマンドいずれでも)。
	void refresh();

private slots:
	void onFilterTextChanged(const QString &text);
	void onItemChanged(QTreeWidgetItem *item, int column);
	void onCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
	void onItemDoubleClicked(QTreeWidgetItem *item, int column);
	void onContextMenuRequested(const QPoint &pos);
	void onFrontSelectionChanged();
	void onBackSelectionChanged();

private:
	ToolContext *m_ctx = nullptr;
	QLineEdit *m_filterEdit;
	QTreeWidget *m_tree;
	bool m_syncingFromScene = false;
	bool m_syncingFromTree = false;
	bool m_rebuilding = false;

	void rebuildTree();
	void jumpTo(const QString &placementUuid, const QString &wireUuid);
	void syncTreeSelectionFromScenes();
};
