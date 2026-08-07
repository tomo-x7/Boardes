#pragma once

// Boardes の内部座標系。
//
// 1 単位 = 0.254mm = PasS の 1px。部品アートワークの 1px と座標が 1:1 対応するため、
// 拡大縮小に由来する座標ズレが原理的に発生しない。
namespace units {

// グリッド関連
constexpr int Pitch = 10;        // フルグリッド (2.54mm)
constexpr int HalfPitch = 5;     // ハーフグリッド (1.27mm)
constexpr int FreeGrid = 1;      // フリー (0.254mm、実質グリッドなし)

// mm 換算
constexpr double MmPerUnit = 0.254;
constexpr double SegmentMm = 2.54;   // 配線1区間 (直交) の長さ
constexpr double DiagonalMm = 3.58;  // 配線1区間 (45度) の長さ

// 既定値
constexpr int DefaultDrillCode = 9;  // ドリルコード0 = 既定0.9mm 相当
constexpr double DrillMmPerCode = 0.1;

inline double toMm(int unitValue) {
	return unitValue * MmPerUnit;
}

inline double drillCodeToMm(int code) {
	return (code == 0 ? DefaultDrillCode : code) * DrillMmPerCode;
}

// スナップ粒度。ツールバーで切り替える対象。
enum class Granularity {
	Full,   // Pitch (10)
	Half,   // HalfPitch (5)
	Free,   // FreeGrid (1)
};

inline int granularityStep(Granularity g) {
	switch (g) {
	case Granularity::Full:
		return Pitch;
	case Granularity::Half:
		return HalfPitch;
	case Granularity::Free:
	default:
		return FreeGrid;
	}
}

}  // namespace units
