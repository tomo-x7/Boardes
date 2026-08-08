#pragma once

#include <QColor>
#include <QGraphicsItem>
#include <QImage>
#include <QVector>

#include "../../model/wire.h"

// ツールが一時的に表示する描画をまとめて受け持つアイテム (モデルには一切影響しない)。
//   - WireTool の「入力中の配線」プレビュー
//   - DraftTool の下書きストローク (保存・エクスポートされない一時レイヤ)
//   - スナップ位置のヒント表示
//   - PlacePartTool の配置プレビュー (ゴースト)
class OverlayItem : public QGraphicsItem {
public:
	explicit OverlayItem(QGraphicsItem *parent = nullptr);

	QRectF boundingRect() const override;
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

	// 入力中の配線: 確定済みの点列 + 現在のカーソル位置。
	void setWirePreview(const QVector<QPoint> &confirmedPoints, QPoint cursorPoint, WireLayer layer);
	void clearWirePreview();

	// 配置ツールの半透明プレビュー。valid=false ならスナップ先が基板外等で赤枠にする。
	void setPlacementGhost(const QImage &image, QPoint pos, QSize size, bool valid);
	void clearPlacementGhost();

	// 下書きモード: 完成したストロークを1本追加する。
	void addDraftStroke(const QVector<QPointF> &points);
	void clearDraft();
	bool hasDraft() const {
		return !m_draftStrokes.isEmpty();
	}

	// 下書きモード: ドラッグ中のストロークをリアルタイムに表示する。
	void setLiveDraftStroke(const QVector<QPointF> &points);
	void clearLiveDraftStroke();

	// スナップ先のヒント (小さな十字/丸)。
	void setSnapHint(QPoint pos, bool visible);

private:
	bool m_hasWirePreview = false;
	QVector<QPoint> m_wirePoints;
	QPoint m_wireCursor;
	WireLayer m_wireLayer = WireLayer::FrontBare;

	QVector<QVector<QPointF>> m_draftStrokes;
	QVector<QPointF> m_liveDraftStroke;

	bool m_snapHintVisible = false;
	QPoint m_snapHintPos;

	bool m_hasGhost = false;
	QImage m_ghostImage;
	QPoint m_ghostPos;
	QSize m_ghostSize;
	bool m_ghostValid = true;

	QRectF m_bounds;
	void recomputeBounds();
};
