#pragma once

#include <QPoint>
#include <QRect>

#include <cmath>

#include "units.h"

// 回転・面など、モデル全体で使う小さな幾何ヘルパー。
enum class Side {
	Front,
	Back,
};

enum class Rotation {
	R0 = 0,
	R90 = 90,
	R180 = 180,
	R270 = 270,
};

inline Rotation rotateCW(Rotation r) {
	switch (r) {
	case Rotation::R0:
		return Rotation::R90;
	case Rotation::R90:
		return Rotation::R180;
	case Rotation::R180:
		return Rotation::R270;
	case Rotation::R270:
	default:
		return Rotation::R0;
	}
}

// 部品原点からの相対座標 (rotation, size は部品アートワークの単位サイズ) を
// 回転後の相対座標に変換する。原点は常に回転後バウンディングボックスの左上。
inline QPoint rotatePoint(QPoint p, QSize size, Rotation r) {
	switch (r) {
	case Rotation::R0:
		return p;
	case Rotation::R90:
		return QPoint(size.height() - p.y(), p.x());
	case Rotation::R180:
		return QPoint(size.width() - p.x(), size.height() - p.y());
	case Rotation::R270:
		return QPoint(p.y(), size.width() - p.x());
	}
	return p;
}

inline QSize rotatedSize(QSize size, Rotation r) {
	if (r == Rotation::R90 || r == Rotation::R270) {
		return QSize(size.height(), size.width());
	}
	return size;
}

inline int snapValue(int v, int step) {
	if (step <= 1) {
		return v;
	}
	return static_cast<int>(std::lround(static_cast<double>(v) / step)) * step;
}

inline QPoint snapPoint(QPoint p, int step) {
	return QPoint(snapValue(p.x(), step), snapValue(p.y(), step));
}
