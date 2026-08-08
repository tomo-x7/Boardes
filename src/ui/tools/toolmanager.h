#pragma once

#include <QObject>
#include <QString>
#include <memory>

#include "../../model/wire.h"
#include "toolcontext.h"

class Tool;
class BoardScene;
class QGraphicsSceneMouseEvent;
class QGraphicsSceneContextMenuEvent;
class QKeyEvent;

// 現在アクティブなツールを保持し、BoardScene から転送されるマウス/キー入力を
// そのツールへ中継する。ツールバーのボタン等から activateXxx() を呼んで切り替える。
class ToolManager : public QObject {
	Q_OBJECT

public:
	explicit ToolManager(ToolContext context, QObject *parent = nullptr);
	~ToolManager() override;

	ToolContext &context() {
		return m_context;
	}

	void activateSelectTool();
	void activatePlacePartTool(const QString &libraryId, const QString &partId);
	void activateWireTool(WireKind kind);
	void activateDraftTool();

	// Esc の共通処理: まずアクティブなツールに途中状態の破棄を試させ (Tool::cancel())、
	// 何も破棄すべきものが無ければ選択ツールへ戻す。MainWindow のグローバル Esc
	// アクションと、BoardScene 経由のキー入力の両方からここへ集約する。
	void cancelCurrent();

	Tool *activeTool() const {
		return m_activeTool.get();
	}

	bool handleMousePress(BoardScene *scene, QGraphicsSceneMouseEvent *event);
	bool handleMouseMove(BoardScene *scene, QGraphicsSceneMouseEvent *event);
	bool handleMouseRelease(BoardScene *scene, QGraphicsSceneMouseEvent *event);
	bool handleMouseDoubleClick(BoardScene *scene, QGraphicsSceneMouseEvent *event);
	bool handleKeyPress(BoardScene *scene, QKeyEvent *event);
	bool handleContextMenu(BoardScene *scene, QGraphicsSceneContextMenuEvent *event);

signals:
	void statusHintChanged(const QString &hint);
	void activeToolChanged();
	// 配置ツールが (Esc/右クリック/他ツールへの切替で) 解除されたとき。PartSelector の
	// 「配置中」表示をクリアするために MainWindow が使う。
	void pendingPartCleared();
	// 選択ツールがアクティブな間、選択状態の要約文字列を中継する
	// (SelectTool::selectionSummaryChanged の転送。選択ツール以外がアクティブなときは空文字)。
	void selectionSummaryChanged(const QString &summary);

private:
	ToolContext m_context;
	std::unique_ptr<Tool> m_activeTool;

	void setTool(std::unique_ptr<Tool> tool);
};
