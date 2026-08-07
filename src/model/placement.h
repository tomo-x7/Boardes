#pragma once

#include <QPoint>
#include <QString>

#include "../core/geometry.h"
#include "part.h"

// 基板上に配置された部品インスタンス。
//
// Part 自体はライブラリ側の不変データなので、Placement は (libraryId, partId) の
// キーで参照するだけで Part へのポインタは持たない (ライブラリ編集・複製で
// Part の実体が差し替わっても Placement 側は影響を受けない設計)。
struct Placement {
	QString uuid;
	QString libraryId;
	QString partId;
	QPoint pos;  // 部品原点の位置 (単位系、回転前の意味で常に左上)
	Rotation rot = Rotation::R0;
	Side side = Side::Front;
	QString refDes;
	QString value;
	bool labelsVisible = true;
	int z = 0;  // 表示順。後に置かれたものが上 (PasS と同じ)
};

// 部品原点からの相対ピン座標を、配置後の絶対座標 (単位系) に変換する。
// side による鏡像化は行わない (鏡像化は描画側でビュー全体に対して行う)。
inline QPoint resolvedPinPosition(QSize partSize, const Placement &placement, QPoint pinRelativePos) {
	return placement.pos + rotatePoint(pinRelativePos, partSize, placement.rot);
}

inline QSize resolvedBoundingSize(QSize partSize, Rotation rot) {
	return rotatedSize(partSize, rot);
}
