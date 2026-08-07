#include "wireitem.h"

#include <QPainter>
#include <QPainterPathStroker>

WireItem::WireItem(std::shared_ptr<const Wire> wire, QGraphicsItem *parent)
	: QGraphicsItem(parent), m_wire(std::move(wire)) {
	recomputeBounds();
	setFlag(QGraphicsItem::ItemIsSelectable, true);
}

QColor WireItem::colorForLayer(WireLayer layer) {
	switch (layer) {
	case WireLayer::FrontBare:
		return QColor(0x20, 0x60, 0xd0);
	case WireLayer::BackBare:
		return QColor(0xd0, 0x50, 0x20);
	case WireLayer::FrontInsulated:
		return QColor(0x60, 0x90, 0xe0);
	case WireLayer::BackInsulated:
		return QColor(0xe0, 0x80, 0x50);
	case WireLayer::Outline:
	default:
		return QColor(0x40, 0x40, 0x40);
	}
}

QPainterPath WireItem::path() const {
	QPainterPath p;
	if (!m_wire || m_wire->points.size() < 2) {
		return p;
	}
	p.moveTo(m_wire->points.first());
	for (int i = 1; i < m_wire->points.size(); ++i) {
		p.lineTo(m_wire->points[i]);
	}
	return p;
}

void WireItem::recomputeBounds() {
	m_bounds = path().boundingRect().adjusted(-2, -2, 2, 2);
}

QRectF WireItem::boundingRect() const {
	return m_bounds;
}

QPainterPath WireItem::shape() const {
	// クリック判定を線の見た目の太さ相当まで広げる (細い1px線だと選択しづらいため)。
	QPainterPathStroker stroker;
	stroker.setWidth(2.0);
	return stroker.createStroke(path());
}

void WireItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
	if (!m_wire) {
		return;
	}
	if (m_netHighlighted) {
		// 選択ハイライトとは別枠の、同一ネット強調用の縁取りを下に敷く。
		QPen halo(QColor(0, 220, 220, 200));
		halo.setWidthF(2.4);
		halo.setCapStyle(Qt::RoundCap);
		halo.setJoinStyle(Qt::RoundJoin);
		painter->setPen(halo);
		painter->drawPath(path());
	}

	QPen pen(colorForLayer(m_wire->layer));
	pen.setWidthF(m_wire->layer == WireLayer::Outline ? 0.6 : 1.0);
	pen.setCapStyle(Qt::RoundCap);
	pen.setJoinStyle(Qt::RoundJoin);
	if (isInsulated(m_wire->layer)) {
		pen.setStyle(Qt::DashLine);
	}
	if (isSelected()) {
		pen.setColor(Qt::yellow);
		pen.setWidthF(pen.widthF() + 0.6);
	}
	painter->setPen(pen);
	painter->drawPath(path());
}

void WireItem::setNetHighlighted(bool on) {
	if (m_netHighlighted == on) {
		return;
	}
	m_netHighlighted = on;
	update();
}

void WireItem::refresh() {
	prepareGeometryChange();
	recomputeBounds();
	update();
}
