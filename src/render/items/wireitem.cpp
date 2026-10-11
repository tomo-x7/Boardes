#include "wireitem.h"

#include <QPainter>
#include <QPainterPathStroker>

WireItem::WireItem(std::shared_ptr<const Wire> wire, QGraphicsItem *parent)
	: QGraphicsItem(parent), m_wire(std::move(wire)) {
	recomputeBounds();
	setFlag(QGraphicsItem::ItemIsSelectable, true);
	// 後から置かれた部品 (z が大きい) の下に隠れないよう、常に部品より上に描く。
	setZValue(zorder::Wire);
	// 実際の表示可否は「レイヤ表示トグル」と「オブジェクト一覧の目玉トグル (wire->visible)」
	// の両方の AND で決まる。両方を知っているのは BoardScene 側なので、初期状態も
	// BoardScene::syncWires() が setVisible() で明示的に設定する (ここでは決めない)。
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
	m_bounds = path().boundingRect().adjusted(-kBoundsMargin, -kBoundsMargin, kBoundsMargin, kBoundsMargin);
}

QRectF WireItem::boundingRect() const {
	return m_bounds;
}

namespace {
// 層ごとの線幅 (モデル単位) と、画面上で保証する最小幅 (px)。
qreal nominalWidth(WireLayer layer) {
	switch (layer) {
	case WireLayer::FrontBare:
	case WireLayer::BackBare:
		return 2.5;  // 約0.64mm (すずめっき線相当)
	case WireLayer::FrontInsulated:
	case WireLayer::BackInsulated:
		return 4.0;  // 約1.0mm (被覆線の外径相当)
	case WireLayer::Outline:
	default:
		return 0.6;
	}
}

qreal minScreenWidth(WireLayer layer) {
	switch (layer) {
	case WireLayer::FrontBare:
	case WireLayer::BackBare:
		return 2.0;
	case WireLayer::FrontInsulated:
	case WireLayer::BackInsulated:
		return 3.0;
	case WireLayer::Outline:
	default:
		return 1.0;
	}
}

// 縁取りを含めた線幅を boundingRect() の余白に収める。
qreal clampToBounds(qreal width) {
	return qMin(width, 2.0 * WireItem::kBoundsMargin);
}
}  // namespace

qreal WireItem::paintWidth(WireLayer layer, qreal px) {
	return clampToBounds(qMax(nominalWidth(layer), minScreenWidth(layer) * px));
}

QPen WireItem::penForLayer(WireLayer layer, qreal px) {
	QPen pen(colorForLayer(layer));
	pen.setWidthF(paintWidth(layer, px));
	pen.setCapStyle(Qt::RoundCap);
	pen.setJoinStyle(Qt::RoundJoin);
	if (isInsulated(layer)) {
		// 丸い線端だと太い破線の隙間が埋まってしまうので、被覆配線は平らな線端にする。
		// 破線のパターンは線幅の倍数なので、太さに関わらず「線幅2つ分の線・1つ分の隙間」。
		pen.setCapStyle(Qt::FlatCap);
		pen.setDashPattern({2.0, 1.0});
	}
	return pen;
}

QPainterPath WireItem::shape() const {
	// クリック判定は線の見た目より少しだけ広げる (ズームに依らないモデル単位)。
	QPainterPathStroker stroker;
	stroker.setWidth(qMax(3.0, nominalWidth(m_wire ? m_wire->layer : WireLayer::FrontBare) + 1.0));
	return stroker.createStroke(path());
}

void WireItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
	if (!m_wire) {
		return;
	}
	const qreal px = 1.0 / qMax(0.001, qAbs(painter->transform().m11()));
	QPen pen = penForLayer(m_wire->layer, px);
	const qreal width = pen.widthF();

	if (m_netHighlighted) {
		// 選択ハイライトとは別枠の、同一ネット強調用の縁取りを下に敷く。
		QPen halo(QColor(0, 220, 220, 200));
		halo.setWidthF(clampToBounds(width + qMax(1.4, 3.0 * px)));
		halo.setCapStyle(Qt::RoundCap);
		halo.setJoinStyle(Qt::RoundJoin);
		painter->setPen(halo);
		painter->drawPath(path());
	}

	if (isSelected()) {
		// 黒縁 (下敷き) + 黄色破線。線の色そのものより優先して視認性を確保する
		// (明るい基板・暗い基板どちらの上でも見えるようにするため)。
		QPen halo(Qt::black);
		halo.setWidthF(clampToBounds(width + 3.0 * px));
		halo.setCapStyle(Qt::RoundCap);
		halo.setJoinStyle(Qt::RoundJoin);
		painter->setPen(halo);
		painter->drawPath(path());

		pen.setColor(Qt::yellow);
		pen.setStyle(Qt::DashLine);
	}
	painter->setPen(pen);
	painter->drawPath(path());

	if (isSelected()) {
		// 頂点にハンドル。太い線に埋もれないよう、線幅より少し大きくする。
		painter->setPen(Qt::NoPen);
		painter->setBrush(Qt::black);
		const qreal hs = qMax(3.5 * px, width + 1.5 * px);
		for (const QPoint &p : m_wire->points) {
			painter->drawRect(QRectF(p.x() - hs / 2, p.y() - hs / 2, hs, hs));
		}
	}
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
