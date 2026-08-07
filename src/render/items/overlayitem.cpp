#include "overlayitem.h"

#include <QPainter>

#include "wireitem.h"

OverlayItem::OverlayItem(QGraphicsItem *parent) : QGraphicsItem(parent) {
	setZValue(1000);  // 常に最前面
}

QRectF OverlayItem::boundingRect() const {
	return m_bounds;
}

void OverlayItem::recomputeBounds() {
	QRectF r;
	if (m_hasWirePreview) {
		for (const auto &p : m_wirePoints) r |= QRectF(p, QSizeF(0, 0));
		r |= QRectF(m_wireCursor, QSizeF(0, 0));
	}
	for (const auto &stroke : m_draftStrokes) {
		for (const auto &p : stroke) r |= QRectF(p, QSizeF(0, 0));
	}
	for (const auto &p : m_liveDraftStroke) r |= QRectF(p, QSizeF(0, 0));
	if (m_snapHintVisible) {
		r |= QRectF(m_snapHintPos, QSizeF(0, 0));
	}
	m_bounds = r.adjusted(-6, -6, 6, 6);
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

void OverlayItem::setSnapHint(QPoint pos, bool visible) {
	prepareGeometryChange();
	m_snapHintPos = pos;
	m_snapHintVisible = visible;
	recomputeBounds();
	update();
}

void OverlayItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
	if (m_hasWirePreview && !m_wirePoints.isEmpty()) {
		QPen pen(WireItem::colorForLayer(m_wireLayer));
		pen.setWidthF(1.0);
		pen.setStyle(Qt::DashLine);
		painter->setPen(pen);
		for (int i = 1; i < m_wirePoints.size(); ++i) {
			painter->drawLine(m_wirePoints[i - 1], m_wirePoints[i]);
		}
		painter->drawLine(m_wirePoints.last(), m_wireCursor);

		painter->setPen(Qt::NoPen);
		painter->setBrush(WireItem::colorForLayer(m_wireLayer));
		for (const auto &p : m_wirePoints) {
			painter->drawEllipse(QPointF(p), 1.0, 1.0);
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

	if (m_snapHintVisible) {
		painter->setPen(QPen(QColor(255, 200, 0), 0.6));
		painter->setBrush(Qt::NoBrush);
		painter->drawEllipse(QPointF(m_snapHintPos), 2.0, 2.0);
	}
}
