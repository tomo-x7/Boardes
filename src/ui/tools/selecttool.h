#pragma once

#include <QObject>
#include <QPoint>
#include <QVector>

#include "../../model/netlist.h"
#include "tool.h"

class QGraphicsSceneContextMenuEvent;

// 選択・移動・回転・表裏切替・削除・コピー&ペーストをまとめて担当するツール。
//
// 選択状態は表裏の BoardScene それぞれが独立した QGraphicsItem を持つため、
// selectionChanged シグナルを介して常に両シーンへミラーする (front で選んだ
// 部品は back 側の対応アイテムも選択済みになる)。
//
// PlacementItem/WireItem には ItemIsMovable を付けていない (Qt の自動移動だと
// Document のモデルデータと Undo を経由しない直接移動になってしまうため)。
// そのためドラッグ移動はこのツールが手動で検出し、スナップと QUndoCommand を
// 適用してから確定する。
class SelectTool : public QObject, public Tool {
	Q_OBJECT

public:
	explicit SelectTool(ToolContext *context);

	void activate() override;
	void deactivate() override;

	bool mousePress(BoardScene *scene, QGraphicsSceneMouseEvent *event) override;
	bool mouseMove(BoardScene *scene, QGraphicsSceneMouseEvent *event) override;
	bool mouseRelease(BoardScene *scene, QGraphicsSceneMouseEvent *event) override;
	bool mouseDoubleClick(BoardScene *scene, QGraphicsSceneMouseEvent *event) override;
	bool keyPress(BoardScene *scene, QKeyEvent *event) override;
	bool contextMenu(BoardScene *scene, QGraphicsSceneContextMenuEvent *event) override;
	QString statusHint() const override;

private slots:
	void onSelectionChanged();

private:
	struct DragEntry {
		QString uuid;
		QPoint startPos;
	};

	bool m_dragging = false;
	QPointF m_dragPressScenePos;
	QPoint m_dragAnchorStart;
	QPoint m_lastDragDelta;
	QVector<DragEntry> m_dragEntries;
	bool m_syncingSelection = false;

	// 同一ネットハイライト用。ホバー対象の配線が変わったときだけ Netlist を再計算する
	// (マウス移動のたびに Union-Find をやり直すのは無駄なため)。
	Netlist m_netlist;
	QString m_lastHoveredWireUuid;
	void updateNetHighlight(BoardScene *scene, QGraphicsSceneMouseEvent *event);
	void clearNetHighlight();

	void syncSelectionAcrossScenes(BoardScene *changedScene);
	QVector<QString> selectedPlacementUuids() const;
	QVector<QString> selectedWireUuids() const;

	void rotateSelected();
	void flipSelected();
	void deleteSelected();
	void nudgeSelected(QPoint delta);
	void copySelected();
	void pasteWithOffset(QPoint offset);
	void pasteAt(QPointF scenePos);
};
