#include "drc.h"

#include <QHash>
#include <QSet>
#include <algorithm>
#include <cmath>

#include "document.h"
#include "librarymanager.h"
#include "netlist.h"
#include "part.h"
#include "placement.h"

namespace {

struct PinRef {
	QString placementUuid;
	int pinNumber = 0;
	int drill = 0;
	QPoint pos;
	bool hasNumber = false;
};

struct ResolvedPlacement {
	std::shared_ptr<Placement> placement;
	std::shared_ptr<Part> part;
	QRect rotatedRect;  // シーン座標系での外形矩形 (アートワークサイズ基準)
};

int effectiveDrill(int drill) {
	return drill == 0 ? units::DefaultDrillCode : drill;
}

// 2線分が「端点以外」で交差するか判定する。交差点が整数格子点ならネット計算側で
// 意図的な接続として扱われるため対象外とする。
bool segmentsCrossAtNonGridPoint(QPoint a1, QPoint a2, QPoint b1, QPoint b2) {
	const double x1 = a1.x(), y1 = a1.y(), x2 = a2.x(), y2 = a2.y();
	const double x3 = b1.x(), y3 = b1.y(), x4 = b2.x(), y4 = b2.y();
	const double denom = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
	if (std::abs(denom) < 1e-9) {
		return false;  // 平行 (完全な重なりのケアは行わない)
	}
	const double t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / denom;
	const double u = ((x1 - x3) * (y1 - y2) - (y1 - y3) * (x1 - x2)) / denom;
	constexpr double kEps = 1e-6;
	if (t <= kEps || t >= 1 - kEps || u <= kEps || u >= 1 - kEps) {
		return false;  // 端点での接触は仕様通りの接続とみなす
	}
	const double ix = x1 + t * (x2 - x1);
	const double iy = y1 + t * (y2 - y1);
	const bool isGridPoint = std::abs(ix - std::round(ix)) < kEps && std::abs(iy - std::round(iy)) < kEps;
	return !isGridPoint;
}

}  // namespace

QVector<DrcFinding> DrcEngine::run(const Document &document, LibraryManager *libraryManager) const {
	QVector<DrcFinding> findings;

	// --- 前準備: 配置ごとの解決済み部品と、穴位置ごとのピン一覧を作る ---
	QVector<ResolvedPlacement> resolved;
	QHash<QPoint, QVector<PinRef>> pinsByPos;
	QSet<QPoint> thruHolePositions;

	for (const auto &placement : document.placements) {
		const auto part =
			libraryManager ? libraryManager->resolvePart(placement->libraryId, placement->partId) : nullptr;
		if (!part) {
			continue;
		}
		ResolvedPlacement rp;
		rp.placement = placement;
		rp.part = part;
		const QSize rotSize = resolvedBoundingSize(part->size(), placement->rot);
		rp.rotatedRect = QRect(placement->pos, rotSize);
		resolved.append(rp);

		for (const auto &pin : part->pins) {
			const QPoint world = resolvedPinPosition(part->size(), *placement, pin.pos);
			pinsByPos[world].append(PinRef{placement->uuid, pin.number, pin.drill, world, pin.hasNumber()});
			if (part->kind == PartKind::ToolThruHole) {
				thruHolePositions.insert(world);
			}
		}
	}

	// --- ルール2・6: 同じ穴の共有 / 異なるドリル径 ---
	for (auto it = pinsByPos.constBegin(); it != pinsByPos.constEnd(); ++it) {
		const auto &pins = it.value();
		QSet<QString> distinctPlacements;
		for (const auto &p : pins) distinctPlacements.insert(p.placementUuid);
		if (distinctPlacements.size() >= 2) {
			DrcFinding f;
			f.severity = DrcSeverity::Error;
			f.ruleId = QStringLiteral("shared-hole");
			f.message = QStringLiteral("%1個の部品が同じ穴 (%2,%3) を共有しています")
							.arg(distinctPlacements.size())
							.arg(it.key().x())
							.arg(it.key().y());
			f.pos = it.key();
			f.relatedPlacementUuid = pins.first().placementUuid;
			findings.append(f);
		}

		const int firstDrill = effectiveDrill(pins.first().drill);
		const bool drillMismatch =
			std::any_of(pins.begin(), pins.end(), [&](const PinRef &p) { return effectiveDrill(p.drill) != firstDrill; });
		if (drillMismatch) {
			DrcFinding f;
			f.severity = DrcSeverity::Warning;
			f.ruleId = QStringLiteral("drill-mismatch");
			f.message =
				QStringLiteral("穴 (%1,%2) に異なるドリル径のピンがあります").arg(it.key().x()).arg(it.key().y());
			f.pos = it.key();
			f.relatedPlacementUuid = pins.first().placementUuid;
			findings.append(f);
		}
	}

	// --- ルール1: 未接続ピン ---
	Netlist netlist;
	netlist.rebuild(document, libraryManager);
	QSet<QPoint> reportedUnconnected;
	for (const auto &rp : resolved) {
		for (const auto &pin : rp.part->pins) {
			if (!pin.hasNumber()) {
				continue;
			}
			const QPoint world = resolvedPinPosition(rp.part->size(), *rp.placement, pin.pos);
			if (reportedUnconnected.contains(world)) {
				continue;
			}
			const int netId = netlist.netIdAt(world, Side::Front);
			if (netId < 0) {
				continue;
			}
			if (netlist.nodesInNet(netId).size() <= 2) {
				reportedUnconnected.insert(world);
				DrcFinding f;
				f.severity = DrcSeverity::Warning;
				f.ruleId = QStringLiteral("unconnected-pin");
				f.message = QStringLiteral("未接続のピンがあります (%1,%2)").arg(world.x()).arg(world.y());
				f.pos = world;
				f.relatedPlacementUuid = rp.placement->uuid;
				findings.append(f);
			}
		}
	}

	// --- ルール3: 部品アウトラインの重なり ---
	for (int i = 0; i < resolved.size(); ++i) {
		for (int j = i + 1; j < resolved.size(); ++j) {
			if (resolved[i].placement->side != resolved[j].placement->side) {
				continue;  // 表裏が違えば重ならない
			}
			if (!resolved[i].rotatedRect.intersects(resolved[j].rotatedRect)) {
				continue;
			}
			DrcFinding f;
			f.severity = DrcSeverity::Warning;
			f.ruleId = QStringLiteral("outline-overlap");
			f.message = QStringLiteral("部品 %1 と %2 のアウトラインが重なっています")
							.arg(resolved[i].placement->refDes.isEmpty() ? resolved[i].placement->uuid
																		  : resolved[i].placement->refDes,
								 resolved[j].placement->refDes.isEmpty() ? resolved[j].placement->uuid
																		  : resolved[j].placement->refDes);
			f.pos = resolved[i].rotatedRect.center();
			f.side = resolved[i].placement->side;
			f.relatedPlacementUuid = resolved[i].placement->uuid;
			findings.append(f);
		}
	}

	// --- ルール4: ピン/配線が基板外形の外 ---
	// (格子上にあることまでは要求しない — PasS 実データにも CN/CND-9 の千鳥配置や
	//  IC/ICMD-18 の 13px ピッチ SMD など、格子に整列しないピンが実在するため)
	const QRect outline = document.board.effectiveOutline();
	for (const auto &rp : resolved) {
		for (const auto &pin : rp.part->pins) {
			const QPoint world = resolvedPinPosition(rp.part->size(), *rp.placement, pin.pos);
			if (outline.contains(world)) {
				continue;
			}
			DrcFinding f;
			f.severity = DrcSeverity::Error;
			f.ruleId = QStringLiteral("outside-board");
			f.message = QStringLiteral("部品 %1 のピンが基板外形の外にあります (%2,%3)")
							.arg(rp.placement->refDes.isEmpty() ? rp.placement->uuid : rp.placement->refDes)
							.arg(world.x())
							.arg(world.y());
			f.pos = world;
			f.side = rp.placement->side;
			f.relatedPlacementUuid = rp.placement->uuid;
			findings.append(f);
		}
	}
	for (const auto &wire : document.wires) {
		for (const auto &p : wire->points) {
			if (!outline.contains(p)) {
				DrcFinding f;
				f.severity = DrcSeverity::Error;
				f.ruleId = QStringLiteral("outside-board");
				f.message = QStringLiteral("配線が基板外形の外にはみ出しています (%1,%2)").arg(p.x()).arg(p.y());
				f.pos = p;
				f.relatedWireUuid = wire->uuid;
				findings.append(f);
				break;
			}
		}
	}

	// --- ルール5: 同じ面の裸線どうしが格子点以外で交差 ---
	struct BareSegment {
		QPoint a, b;
		QString wireUuid;
	};
	QHash<int, QVector<BareSegment>> bareSegmentsBySide;  // Side を int にして使う
	for (const auto &wire : document.wires) {
		if (isInsulated(wire->layer) || wire->layer == WireLayer::Outline) {
			continue;
		}
		const auto side = wireSide(wire->layer);
		if (!side.has_value()) {
			continue;
		}
		auto &segs = bareSegmentsBySide[static_cast<int>(*side)];
		for (int i = 1; i < wire->points.size(); ++i) {
			segs.append(BareSegment{wire->points[i - 1], wire->points[i], wire->uuid});
		}
	}
	for (auto it = bareSegmentsBySide.constBegin(); it != bareSegmentsBySide.constEnd(); ++it) {
		const auto &segs = it.value();
		for (int i = 0; i < segs.size(); ++i) {
			for (int j = i + 1; j < segs.size(); ++j) {
				if (segmentsCrossAtNonGridPoint(segs[i].a, segs[i].b, segs[j].a, segs[j].b)) {
					DrcFinding f;
					f.severity = DrcSeverity::Error;
					f.ruleId = QStringLiteral("wire-short");
					f.message = QStringLiteral("配線同士が格子点以外で交差しています (ショートの可能性)");
					f.pos = segs[i].a;
					f.side = static_cast<Side>(it.key());
					f.relatedWireUuid = segs[i].wireUuid;
					findings.append(f);
				}
			}
		}
	}

	// --- ルール7: 両面基板で表面配線の端点にピン/スルーホールが無い ---
	if (document.board.doubleSided) {
		for (const auto &wire : document.wires) {
			if (wire->layer != WireLayer::FrontBare && wire->layer != WireLayer::FrontInsulated) {
				continue;
			}
			if (wire->points.isEmpty()) {
				continue;
			}
			for (const QPoint &endpoint : {wire->points.first(), wire->points.last()}) {
				const bool hasPin = pinsByPos.contains(endpoint) &&
									std::any_of(pinsByPos[endpoint].begin(), pinsByPos[endpoint].end(),
												[](const PinRef &p) { return p.hasNumber; });
				if (hasPin || thruHolePositions.contains(endpoint)) {
					continue;
				}
				DrcFinding f;
				f.severity = DrcSeverity::Warning;
				f.ruleId = QStringLiteral("front-wire-floating-endpoint");
				f.message = QStringLiteral("表面配線の端点 (%1,%2) にピンもスルーホールもありません")
								.arg(endpoint.x())
								.arg(endpoint.y());
				f.pos = endpoint;
				f.side = Side::Front;
				f.relatedWireUuid = wire->uuid;
				findings.append(f);
			}
		}
	}

	return findings;
}
