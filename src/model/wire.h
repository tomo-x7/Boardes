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

// ユーザーが配線ツールで選ぶのは「線の種類」だけ。実際の面 (表/裏) はどちらの
// エディタ (BoardScene::side()) で描いたかによって決まる方が直感的なので、
// WireTool はこちらを引数に取る。WireLayer 自体 (モデル・保存形式) は変えない。
enum class WireKind {
	Bare,       // 裸線
	Insulated,  // 被覆配線
	Outline,    // 外形線 (Side に関わらず WireLayer::Outline)
};

inline WireLayer wireLayerFor(WireKind kind, Side side) {
	switch (kind) {
	case WireKind::Bare:
		return side == Side::Back ? WireLayer::BackBare : WireLayer::FrontBare;
	case WireKind::Insulated:
		return side == Side::Back ? WireLayer::BackInsulated : WireLayer::FrontInsulated;
	case WireKind::Outline:
	default:
		return WireLayer::Outline;
	}
}

inline WireKind wireKindOf(WireLayer layer) {
	switch (layer) {
	case WireLayer::BackBare:
	case WireLayer::FrontBare:
		return WireKind::Bare;
	case WireLayer::BackInsulated:
	case WireLayer::FrontInsulated:
		return WireKind::Insulated;
	case WireLayer::Outline:
	default:
		return WireKind::Outline;
	}
}

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
	bool visible = true;  // 表示/非表示 (オブジェクト一覧の目玉トグル用。false でも削除ではない)
};
