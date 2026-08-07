#pragma once

#include <QPoint>
#include <QVector>

#include "../../model/wire.h"
#include "tool.h"

// クリックで頂点を追加していくポリライン配線ツール。右クリックまたは
// ダブルクリックで確定、Escape で破棄する。描画を開始した面 (表/裏どちらの
// BoardScene で最初にクリックしたか) をまたぐことはできない。
class WireTool : public Tool {
public:
	WireTool(ToolContext *context, WireLayer layer);

	void deactivate() override;
	bool mousePress(BoardScene *scene, QGraphicsSceneMouseEvent *event) override;
	bool mouseMove(BoardScene *scene, QGraphicsSceneMouseEvent *event) override;
	bool mouseRelease(BoardScene *scene, QGraphicsSceneMouseEvent *event) override;
	bool mouseDoubleClick(BoardScene *scene, QGraphicsSceneMouseEvent *event) override;
	bool keyPress(BoardScene *scene, QKeyEvent *event) override;
	QString statusHint() const override;

private:
	WireLayer m_layer;
	BoardScene *m_activeScene = nullptr;
	QVector<QPoint> m_points;

	// 1点目は自由な位置にスナップし、2点目以降は直前の頂点から見て8方向 (直交/45度)
	// にスナップする (PasS の配線規則、かつ Netlist の「触れたら繋がる」判定の前提)。
	QPoint snapNext(QPointF scenePos) const;
	void finish(bool commit);
};
