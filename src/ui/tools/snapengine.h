#pragma once

#include <QPoint>
#include <QPointF>
#include <cmath>

#include "../../core/geometry.h"
#include "../../core/units.h"
#include "../../model/board.h"

// スナップ粒度を保持し、モデル座標を基板の格子点に丸める。
//
// 部品配置は PasS に合わせて常にフルグリッド固定、配線だけがツールバーで
// 選んだ粒度 (フル/ハーフ/フリー) に従う。丸めの基準点は基板の origin
// (外形左上から最初の穴中心までのオフセット) であり、シーン原点 (0,0) では
// ない。基板の origin が pitch の半分 (例: 5) の場合、これを無視して
// シーン原点基準に丸めると常に半グリッドずれる。
class SnapEngine {
public:
	// 非所有。Document::board の実体アドレスを渡す (Document 切替時に呼び直すこと)。
	void setBoard(const BoardSpec *board) {
		m_board = board;
	}

	void setGranularity(units::Granularity g) {
		m_granularity = g;
	}
	units::Granularity granularity() const {
		return m_granularity;
	}

	// 基板の pitch (無ければ units::Pitch) を基準にした、現在の粒度での刻み幅。
	int step() const {
		const int pitch = m_board ? m_board->pitch : units::Pitch;
		switch (m_granularity) {
		case units::Granularity::Full:
			return pitch;
		case units::Granularity::Half:
			return qMax(1, pitch / 2);
		case units::Granularity::Free:
		default:
			return units::FreeGrid;
		}
	}

	// 部品配置は常にフル格子。
	int placementStep() const {
		return m_board ? m_board->pitch : units::Pitch;
	}

	QPoint origin() const {
		return m_board ? m_board->origin : QPoint(0, 0);
	}

	// モデル座標を「基板の格子点」に丸める。board が null のときは origin=(0,0) として
	// 従来どおり丸める。
	QPoint snapToGrid(QPointF modelPos, int step) const {
		const QPoint o = origin();
		const QPointF rel = modelPos - QPointF(o);
		return o + snapPoint(QPoint(qRound(rel.x()), qRound(rel.y())), step);
	}

	// 最初の頂点や、方向を問わない単純なスナップに使う。
	QPoint snapForWire(QPointF modelPos) const {
		return snapToGrid(modelPos, step());
	}

	// 2点目以降の頂点用。PasS の配線は常に直交または45度なので、from から見た
	// 角度を8方向 (45度刻み) にスナップしてから、その方向に沿った距離を
	// グリッド単位に丸める。ネットの「触れたら繋がる」判定 (Netlist) は線分が
	// 必ずこの8方向のどれかであることを前提にしているため、描画時点でこの制約を
	// かけておく必要がある。
	QPoint snapForWireVertex(QPoint from, QPointF modelTarget) const {
		const QPointF delta = modelTarget - QPointF(from);
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
		const int s = step();
		const int numSteps = qMax(0, qRound(projected / s));

		return from + QPoint(kDirX[octant] * numSteps * s, kDirY[octant] * numSteps * s);
	}

	// 部品配置: anchor (回転後、部品原点からの相対座標) が格子点に一致するような
	// 部品原点 (左上) を返す。
	QPoint snapForPlacement(QPointF modelCursor, QPoint rotatedAnchor) const {
		return snapToGrid(modelCursor, placementStep()) - rotatedAnchor;
	}

private:
	const BoardSpec *m_board = nullptr;
	units::Granularity m_granularity = units::Granularity::Full;
};
