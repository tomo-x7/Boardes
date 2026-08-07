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
	void activateWireTool(WireLayer layer);
	void activateDraftTool();

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

private:
	ToolContext m_context;
	std::unique_ptr<Tool> m_activeTool;

	void setTool(std::unique_ptr<Tool> tool);
};
