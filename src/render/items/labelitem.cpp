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
	if (m_counterMirrored) {
		setTransform(QTransform(-1, 0, 0, 1, m_bounds.width(), 0));
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

void LabelItem::setCounterMirrored(bool on) {
	if (m_counterMirrored == on) {
		return;
	}
	m_counterMirrored = on;
	applyMirrorIfNeeded();
}
