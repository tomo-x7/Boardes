#include "overlayitem.h"

#include <QPainter>

#include "wireitem.h"

OverlayItem::OverlayItem(QGraphicsItem *parent) : QGraphicsItem(parent) {
	setZValue(1000);  // 常に最前面
}

QRectF OverlayItem::boundingRect() const {
	return m_bounds;
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
	m_bounds = hasAny ? r.adjusted(-6, -6, 6, 6) : QRectF();
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
}

void OverlayItem::clearDraft() {
	if (m_draftStrokes.isEmpty()) {
		return;
	}
	prepareGeometryChange();
	m_draftStrokes.clear();
	recomputeBounds();
	update();
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
		// 破線・頂点マーカーは他のインタラクティブな装飾 (WireItem の選択縁取り・
		// PlacementItem の選択枠など) と同じく、ズームしても画面上のサイズが
		// 変わらない「デバイス非依存の1px」単位 (px) で幅を決める。以前はモデル
		// 単位の固定値だったため、拡大すると太くなりすぎ・縮小すると見えなくなり、
		// 他の装飾と見た目の太さが揃っていなかった。
		const qreal px = 1.0 / qMax(0.001, qAbs(painter->transform().m11()));

		QPen pen(WireItem::colorForLayer(m_wireLayer));
		pen.setWidthF(1.2 * px);
		pen.setStyle(Qt::DashLine);
		painter->setPen(pen);
		for (int i = 1; i < m_wirePoints.size(); ++i) {
			painter->drawLine(m_wirePoints[i - 1], m_wirePoints[i]);
		}
		painter->drawLine(m_wirePoints.last(), m_wireCursor);

		painter->setPen(Qt::NoPen);
		painter->setBrush(WireItem::colorForLayer(m_wireLayer));
		const qreal r = 1.5 * px;
		for (const auto &p : m_wirePoints) {
			painter->drawEllipse(QPointF(p), r, r);
		}
	}

	if (!m_draftStrokes.isEmpty() || !m_liveDraftStroke.isEmpty()) {
		QPen pen(QColor(80, 80, 200, 200));
		pen.setWidthF(0.8);
		pen.setCapStyle(Qt::RoundCap);
		pen.setJoinStyle(Qt::RoundJoin);
		painter->setPen(pen);
		for (const auto &stroke : m_draftStrokes) {
			for (int i = 1; i < stroke.size(); ++i) {
				painter->drawLine(stroke[i - 1], stroke[i]);
			}
		}
		for (int i = 1; i < m_liveDraftStroke.size(); ++i) {
			painter->drawLine(m_liveDraftStroke[i - 1], m_liveDraftStroke[i]);
		}
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
