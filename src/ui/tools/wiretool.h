#pragma once

#include <QPoint>
#include <QVector>

#include "../../model/wire.h"
#include "tool.h"

// クリックで頂点を追加していくポリライン配線ツール。右クリックまたは Enter で
// 確定、Escape で破棄する。描画を開始した面 (表/裏どちらの BoardScene で最初に
// クリックしたか) をまたぐことはできない。実際のレイヤ (表/裏) はどちらの
// エディタで描いたかによって決まる (WireKind + BoardScene::side())。
class WireTool : public Tool {
public:
	WireTool(ToolContext *context, WireKind kind);

	void deactivate() override;
	bool mousePress(BoardScene *scene, QGraphicsSceneMouseEvent *event) override;
	bool mouseMove(BoardScene *scene, QGraphicsSceneMouseEvent *event) override;
	bool mouseRelease(BoardScene *scene, QGraphicsSceneMouseEvent *event) override;
	bool keyPress(BoardScene *scene, QKeyEvent *event) override;
	void mouseLeave(BoardScene *scene) override;
	bool cancel() override;
	QString statusHint(const Keymap &keymap) const override;

private:
	WireKind m_kind;
	BoardScene *m_activeScene = nullptr;
	QVector<QPoint> m_points;

	// 1点目は自由な位置にスナップし、2点目以降は直前の頂点から見て8方向 (直交/45度)
	// にスナップする (PasS の配線規則、かつ Netlist の「触れたら繋がる」判定の前提)。
	QPoint snapNext(QPointF modelPos) const;
	void finish(bool commit);
};
