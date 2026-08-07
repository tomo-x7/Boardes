#pragma once

#include <QGraphicsItem>
#include <memory>

#include "../../model/wire.h"

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
