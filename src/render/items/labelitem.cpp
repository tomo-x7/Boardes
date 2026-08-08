#include "labelitem.h"

#include <QFont>
#include <QFontMetricsF>
#include <QPainter>

namespace {
constexpr int kLabelPixelSize = 4;  // 単位系での文字高さ (0.254mm/単位 なので約1mm)
}

LabelItem::LabelItem(QGraphicsItem *parent) : QGraphicsItem(parent) {
	recomputeBounds();
}

QString LabelItem::displayText() const {
	if (m_value.isEmpty()) {
		return m_refDes;
	}
	if (m_refDes.isEmpty()) {
		return m_value;
	}
	return m_refDes + QLatin1Char('\n') + m_value;
}

void LabelItem::recomputeBounds() {
	QFont f;
	f.setPixelSize(kLabelPixelSize);
	const QFontMetricsF fm(f);
	const QString text = displayText();
	m_bounds = text.isEmpty() ? QRectF() : fm.boundingRect(QRectF(), Qt::TextWordWrap, text);
	m_bounds.adjust(-0.5, -0.5, 0.5, 0.5);
	applyMirrorIfNeeded();
}

void LabelItem::applyMirrorIfNeeded() {
	const qreal sx = m_flipX ? -1 : 1;
	const qreal sy = m_flipY ? -1 : 1;
	const qreal dx = m_flipX ? m_bounds.width() : 0;
	const qreal dy = m_flipY ? m_bounds.height() : 0;
	if (m_flipX || m_flipY) {
		setTransform(QTransform(sx, 0, 0, sy, dx, dy));
	} else {
		setTransform(QTransform());
	}
}

QRectF LabelItem::boundingRect() const {
	return m_bounds;
}

void LabelItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
	const QString text = displayText();
	if (text.isEmpty()) {
		return;
	}
	QFont f = painter->font();
	f.setPixelSize(kLabelPixelSize);
	painter->setFont(f);
	painter->setPen(Qt::black);
	painter->drawText(m_bounds, Qt::TextWordWrap, text);
}

void LabelItem::setTexts(const QString &refDes, const QString &value) {
	if (m_refDes == refDes && m_value == value) {
		return;
	}
	prepareGeometryChange();
	m_refDes = refDes;
	m_value = value;
	recomputeBounds();
	update();
}

void LabelItem::setAnchor(QPointF unitPos) {
	setPos(unitPos);
}

void LabelItem::setCounterMirror(bool flipX, bool flipY) {
	if (m_flipX == flipX && m_flipY == flipY) {
		return;
	}
	m_flipX = flipX;
	m_flipY = flipY;
	applyMirrorIfNeeded();
}
