#include "overlayitem.h"

#include <QPainter>

#include "wireitem.h"

OverlayItem::OverlayItem(QGraphicsItem *parent) : QGraphicsItem(parent) {
	// 常に最前面 (配線よりさらに上)
	setZValue(zorder::Overlay);
}

QRectF OverlayItem::boundingRect() const {
	return m_bounds;
}

QPainterPath OverlayItem::shape() const {
	// オーバーレイは見た目だけの描画面で、クリックや選択の対象ではない。既定の shape() は
	// boundingRect() 全体で、しかもこのアイテムは最前面にある。そのままだと、下書きの線や
	// プレビューを囲む矩形の中にある部品・配線が、SelectTool の itemAt() によるヒット
	// テストで隠れて、クリック・ドラッグ・右クリックメニューが効かなくなる。
	return QPainterPath();
}

namespace {
// QRectF::operator| は片方が isNull() (幅0 かつ 高さ0) だと、結合せずもう片方を
// そのまま返してしまう。点だけを集めて外接矩形を求めたいときにこれを使うと
// 常に最後の点だけが残ってしまうため、min/max を手動で積算する。
void extendBounds(QRectF &r, bool &hasAny, const QPointF &p) {
	if (!hasAny) {
		r = QRectF(p, QSizeF(0, 0));
		hasAny = true;
		return;
	}
	if (p.x() < r.left()) r.setLeft(p.x());
	if (p.x() > r.right()) r.setRight(p.x());
	if (p.y() < r.top()) r.setTop(p.y());
	if (p.y() > r.bottom()) r.setBottom(p.y());
}
}  // namespace

void OverlayItem::recomputeBounds() {
	QRectF r;
	bool hasAny = false;
	if (m_hasWirePreview) {
		for (const auto &p : m_wirePoints) extendBounds(r, hasAny, p);
		extendBounds(r, hasAny, m_wireCursor);
	}
	for (const auto &stroke : m_draftStrokes) {
		for (const auto &p : stroke) extendBounds(r, hasAny, p);
	}
	for (const auto &p : m_liveDraftStroke) extendBounds(r, hasAny, p);
	if (m_hasGhost) {
		r |= QRectF(m_ghostPos, QSizeF(m_ghostSize));
		hasAny = true;
	}
	// 配線プレビューは WireItem と同じ太さで描くので、余白も WireItem に揃える。
	const qreal margin = WireItem::kBoundsMargin;
	m_bounds = hasAny ? r.adjusted(-margin, -margin, margin, margin) : QRectF();
}

void OverlayItem::setWirePreview(const QVector<QPoint> &confirmedPoints, QPoint cursorPoint, WireLayer layer) {
	prepareGeometryChange();
	m_hasWirePreview = true;
	m_wirePoints = confirmedPoints;
	m_wireCursor = cursorPoint;
	m_wireLayer = layer;
	recomputeBounds();
	update();
}

void OverlayItem::clearWirePreview() {
	if (!m_hasWirePreview) {
		return;
	}
	prepareGeometryChange();
	m_hasWirePreview = false;
	m_wirePoints.clear();
	recomputeBounds();
	update();
}

void OverlayItem::addDraftStroke(const QVector<QPointF> &points) {
	if (points.size() < 2) {
		return;
	}
	prepareGeometryChange();
	m_draftStrokes.append(points);
	recomputeBounds();
	update();
	if (m_draftChanged) {
		m_draftChanged();
	}
}

void OverlayItem::clearDraft() {
	if (m_draftStrokes.isEmpty()) {
		return;
	}
	prepareGeometryChange();
	m_draftStrokes.clear();
	recomputeBounds();
	update();
	if (m_draftChanged) {
		m_draftChanged();
	}
}

void OverlayItem::setLiveDraftStroke(const QVector<QPointF> &points) {
	prepareGeometryChange();
	m_liveDraftStroke = points;
	recomputeBounds();
	update();
}

void OverlayItem::clearLiveDraftStroke() {
	if (m_liveDraftStroke.isEmpty()) {
		return;
	}
	prepareGeometryChange();
	m_liveDraftStroke.clear();
	recomputeBounds();
	update();
}

void OverlayItem::setPlacementGhost(const QImage &image, QPoint pos, QSize size, bool valid) {
	prepareGeometryChange();
	m_hasGhost = true;
	m_ghostImage = image;
	m_ghostPos = pos;
	m_ghostSize = size;
	m_ghostValid = valid;
	recomputeBounds();
	update();
}

void OverlayItem::clearPlacementGhost() {
	if (!m_hasGhost) {
		return;
	}
	prepareGeometryChange();
	m_hasGhost = false;
	recomputeBounds();
	update();
}

void OverlayItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
	if (m_hasWirePreview && !m_wirePoints.isEmpty()) {
		// 確定後の配線 (WireItem) と同じ太さ・線種で描き、少し透かして「入力中」と分かるようにする。
		// カーソルへ伸びる最後の区間だけは、まだ頂点が決まっていないので破線にする。
		const qreal px = 1.0 / qMax(0.001, qAbs(painter->transform().m11()));
		painter->save();
		painter->setOpacity(0.75);
		QPen pen = WireItem::penForLayer(m_wireLayer, px);
		painter->setPen(pen);
		// 半透明なので、区間ごとに drawLine すると継ぎ目が二重に塗られて濃くなる。1本の折れ線で描く。
		painter->drawPolyline(m_wirePoints.constData(), m_wirePoints.size());
		pen.setCapStyle(Qt::FlatCap);
		pen.setDashPattern({2.0, 1.0});
		painter->setPen(pen);
		painter->drawLine(m_wirePoints.last(), m_wireCursor);

		painter->setPen(Qt::NoPen);
		painter->setBrush(WireItem::colorForLayer(m_wireLayer));
		const qreal r = qMin(qMax(1.5 * px, pen.widthF() * 0.75), WireItem::kBoundsMargin);
		for (const auto &p : m_wirePoints) {
			painter->drawEllipse(QPointF(p), r, r);
		}
		painter->restore();
	}

	if (!m_draftStrokes.isEmpty() || !m_liveDraftStroke.isEmpty()) {
		QPen pen(QColor(80, 80, 200, 200));
		pen.setWidthF(0.8);
		pen.setCapStyle(Qt::RoundCap);
		pen.setJoinStyle(Qt::RoundJoin);
		painter->setPen(pen);
		// 1本のストロークは1つの折れ線として描く。区間ごとに drawLine すると、半透明のペンでは
		// 区間の継ぎ目 (丸いキャップの重なり) が二重に塗られて、点々と濃く見えてしまう。
		for (const auto &stroke : m_draftStrokes) {
			painter->drawPolyline(stroke.constData(), stroke.size());
		}
		painter->drawPolyline(m_liveDraftStroke.constData(), m_liveDraftStroke.size());
	}

	if (m_hasGhost) {
		painter->save();
		painter->setOpacity(0.55);
		const QRectF target = QRectF(QPointF(m_ghostPos), QSizeF(m_ghostSize));
		if (!m_ghostImage.isNull()) {
			painter->drawImage(target, m_ghostImage, QRectF(m_ghostImage.rect()));
		}
		painter->setOpacity(1.0);
		QPen pen(m_ghostValid ? QColor(80, 220, 120) : QColor(230, 60, 60));
		pen.setWidthF(0.8);
		pen.setStyle(Qt::DashLine);
		painter->setPen(pen);
		painter->setBrush(Qt::NoBrush);
		painter->drawRect(target);
		painter->restore();
	}
}
