#pragma once

#include <QPoint>
#include <QString>
#include <QVector>
#include <optional>

#include "../core/geometry.h"

// PasS 準拠の4配線種 + 外形線。
enum class WireLayer {
	BackBare,        // 裏面配線
	FrontBare,       // 表面配線
	BackInsulated,   // 裏面被覆配線
	FrontInsulated,  // 表面被覆配線
	Outline,         // 基板外形線
};

QString wireLayerToString(WireLayer layer);
WireLayer wireLayerFromString(const QString &s);

inline bool isInsulated(WireLayer layer) {
	return layer == WireLayer::BackInsulated || layer == WireLayer::FrontInsulated;
}

// 配線が属する面。Outline はどちらの面にも属さない (両面に描画される)。
inline std::optional<Side> wireSide(WireLayer layer) {
	switch (layer) {
	case WireLayer::BackBare:
	case WireLayer::BackInsulated:
		return Side::Back;
	case WireLayer::FrontBare:
	case WireLayer::FrontInsulated:
		return Side::Front;
	case WireLayer::Outline:
	default:
		return std::nullopt;
	}
}

// 配線 (ポリライン)。points は単位系の絶対座標で2点以上。
struct Wire {
	QString uuid;
	WireLayer layer = WireLayer::FrontBare;
	QVector<QPoint> points;
};
