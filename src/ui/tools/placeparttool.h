#pragma once

#include "tool.h"

// 部品パレットで選択された部品を、クリックした位置に配置する。
// 配置は常にフルグリッドにスナップする (PasS 仕様)。R キーで回転しながら
// 連続配置できる (Escape や他ツールへの切替まで有効)。
class PlacePartTool : public Tool {
public:
	explicit PlacePartTool(ToolContext *context) : Tool(context) {
	}

	bool mousePress(BoardScene *scene, QGraphicsSceneMouseEvent *event) override;
	bool mouseRelease(BoardScene *scene, QGraphicsSceneMouseEvent *event) override;
	bool keyPress(BoardScene *scene, QKeyEvent *event) override;
	QString statusHint() const override;
};
