#pragma once

#include <QPoint>
#include <QPointF>
#include <cmath>

#include "../../core/geometry.h"
#include "../../core/units.h"

// スナップ粒度を保持し、シーン座標を単位系のグリッド点に丸める。
//
// 部品配置は PasS に合わせて常にフルグリッド固定、配線だけがツールバーで
// 選んだ粒度 (フル/ハーフ/フリー) に従う。
class SnapEngine {
public:
	void setGranularity(units::Granularity g) {
		m_granularity = g;
	}
	units::Granularity granularity() const {
		return m_granularity;
	}

	// 最初の頂点や、方向を問わない単純なスナップに使う。
	QPoint snapForWire(QPointF scenePos) const {
		return snapPoint(roundToPoint(scenePos), units::granularityStep(m_granularity));
	}

	// 2点目以降の頂点用。PasS の配線は常に直交または45度なので、from から見た
	// 角度を8方向 (45度刻み) にスナップしてから、その方向に沿った距離を
	// グリッド単位に丸める。ネットの「触れたら繋がる」判定 (Netlist) は線分が
	// 必ずこの8方向のどれかであることを前提にしているため、描画時点でこの制約を
	// かけておく必要がある。
	QPoint snapForWireVertex(QPoint from, QPointF targetScenePos) const {
		const QPointF delta = targetScenePos - QPointF(from);
		if (qAbs(delta.x()) < 0.5 && qAbs(delta.y()) < 0.5) {
			return from;
		}
		static constexpr int kDirX[8] = {1, 1, 0, -1, -1, -1, 0, 1};
		static constexpr int kDirY[8] = {0, 1, 1, 1, 0, -1, -1, -1};

		const double angle = std::atan2(delta.y(), delta.x());
		int octant = static_cast<int>(std::lround(angle / (M_PI / 4.0))) % 8;
		if (octant < 0) {
			octant += 8;
		}
		const double dirLen = std::hypot(kDirX[octant], kDirY[octant]);  // 1 または sqrt(2)
		const double projected = (delta.x() * kDirX[octant] + delta.y() * kDirY[octant]) / dirLen;
		const int step = units::granularityStep(m_granularity);
		const int numSteps = qMax(0, qRound(projected / step));

		return from + QPoint(kDirX[octant] * numSteps * step, kDirY[octant] * numSteps * step);
	}

	static QPoint snapForPlacement(QPointF scenePos) {
		return snapPoint(roundToPoint(scenePos), units::Pitch);
	}

private:
	units::Granularity m_granularity = units::Granularity::Full;

	static QPoint roundToPoint(QPointF p) {
		return QPoint(qRound(p.x()), qRound(p.y()));
	}
};
