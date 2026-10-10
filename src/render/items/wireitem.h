#pragma once

#include <QGraphicsItem>
#include <memory>

#include "../../model/wire.h"

// 同じ親 (BoardScene の rootItem) の下に並ぶアイテムの描画順 (Z 値)。部品の z は
// Document::nextZValue() (既存の最大値+1、上限なし) で決まるので、配線とオーバーレイは
// int の最大値 (約2.1e9) より大きい値にして、部品をいくつ置いても
// 「部品 < 配線 < オーバーレイ (入力中のプレビュー等)」の順を保つ。
namespace zorder {
constexpr qreal Wire = 1.0e10;
constexpr qreal Overlay = 2.0e10;
}  // namespace zorder

// 1本の配線 (ポリライン) を描画するアイテム。層ごとに線種・色を変える。
class WireItem : public QGraphicsItem {
public:
	explicit WireItem(std::shared_ptr<const Wire> wire, QGraphicsItem *parent = nullptr);

	QRectF boundingRect() const override;
	QPainterPath shape() const override;
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

	std::shared_ptr<const Wire> wire() const {
		return m_wire;
	}
	void refresh();

	// 同一ネットハイライト (選択とは別に、ホバー中の配線と同じネットを強調する)。
	void setNetHighlighted(bool on);
	bool isNetHighlighted() const {
		return m_netHighlighted;
	}

	static QColor colorForLayer(WireLayer layer);

private:
	std::shared_ptr<const Wire> m_wire;
	QRectF m_bounds;
	bool m_netHighlighted = false;

	QPainterPath path() const;
	void recomputeBounds();
};
