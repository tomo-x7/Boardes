#pragma once

#include "tool.h"

// 部品パレットで選択された部品を、クリックした位置に配置する。
// 配置は常にフルグリッドにスナップする (PasS 仕様)。左クリックで連続配置でき、
// R キーで回転、右クリックまたは Esc でツールを解除して選択ツールに戻る。
class PlacePartTool : public Tool {
public:
	explicit PlacePartTool(ToolContext *context) : Tool(context) {
	}

	bool mousePress(BoardScene *scene, QGraphicsSceneMouseEvent *event) override;
	bool mouseMove(BoardScene *scene, QGraphicsSceneMouseEvent *event) override;
	bool mouseRelease(BoardScene *scene, QGraphicsSceneMouseEvent *event) override;
	bool keyPress(BoardScene *scene, QKeyEvent *event) override;
	bool contextMenu(BoardScene *scene, QGraphicsSceneContextMenuEvent *event) override;
	void deactivate() override;
	QString statusHint(const Keymap &keymap) const override;
};
