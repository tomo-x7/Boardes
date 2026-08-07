#pragma once

#include <QGraphicsItem>
#include <QPixmap>

#include "../../core/geometry.h"
#include "../../model/board.h"

// 基板の背景・格子・ランド・銅箔を描画するアイテム。
//
// PasS 取込基板 (背景ラスタあり) はその画像をそのまま描き、
// パラメトリック基板 (背景なし) は substrateColor 塗り + 銅箔 + ランド + 穴を
// コードで生成する。どちらのパスでも結果は1枚の QPixmap にキャッシュしてから
// 描画するので、再描画のたびに図形計算をやり直さない。
class BoardItem : public QGraphicsItem {
public:
	BoardItem(const BoardSpec *board, Side side, QGraphicsItem *parent = nullptr);

	QRectF boundingRect() const override;
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

	void setBoard(const BoardSpec *board);
	void setSide(Side side);

private:
	const BoardSpec *m_board;  // 非所有 (Document::board を指す。Document と同じ寿命)
	Side m_side;
	QPixmap m_cache;

	void rebuild();
};
