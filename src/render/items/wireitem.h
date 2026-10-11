#pragma once

#include <QGraphicsItem>
#include <QPen>
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

	// 実際に描く線幅 (モデル単位)。基本はモデル単位の太さ (裸線 2.5 = 約0.64mm、被覆配線 4.0 =
	// 約1.0mm) で、縮小表示しても画面上で細くなりすぎないよう最小の画面ピクセル幅を保証する。
	// px は「画面1ピクセルに相当するモデル単位」(1 / 表示倍率)。入力中のプレビュー
	// (OverlayItem) も同じ幅で描き、確定後と太さが変わらないようにする。
	static qreal paintWidth(WireLayer layer, qreal px);
	// 層ごとの色・線種・paintWidth() をまとめたペン。
	static QPen penForLayer(WireLayer layer, qreal px);
	// 描画が boundingRect() からはみ出さないよう、線幅 (縁取りを含む) の半分をこれ以下に抑える。
	static constexpr qreal kBoundsMargin = 6.0;

private:
	std::shared_ptr<const Wire> m_wire;
	QRectF m_bounds;
	bool m_netHighlighted = false;

	QPainterPath path() const;
	void recomputeBounds();
};
