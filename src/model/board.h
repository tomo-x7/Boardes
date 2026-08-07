#pragma once

#include <QColor>
#include <QPair>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>
#include <QVector>
#include <optional>

#include "part.h"  // Artwork の再利用

// ランド (穴の周りの銅箔パッド) の形状。
enum class PadShape {
	None,    // ランドなし (PasS 取込基板など、背景画像がそのまま銅箔を表現する場合)
	Round,
	Square,
};
QString padShapeToString(PadShape s);
PadShape padShapeFromString(const QString &s);

// 銅箔パターン。
enum class CopperPattern {
	None,             // 銅箔レイヤなし (背景画像に含まれる、または部品面のみ)
	PadPerHole,       // 穴ごとに独立したランド (蛇の目基板)
	StripHorizontal,  // 横方向ストライプ (ベロボード)
	StripVertical,    // 縦方向ストライプ
};
QString copperPatternToString(CopperPattern c);
CopperPattern copperPatternFromString(const QString &s);

// 基板の色の既定値。ここ1箇所だけで定義し、BoardSpec のメンバ初期化子・
// boardio.cpp の JSON デシリアライズ時フォールバック・BoardEditorDialog の
// UI 初期値がすべてこれを参照する (重複させると片方だけ変えたときに
// 静かに食い違うため)。substrate は Boardes 独自の FR-4 グリーンであり、
// PasS のクロマキー色 (187,201,158) とは無関係。
namespace boarddefaults {
inline const QColor Substrate{0x2F, 0x6B, 0x4F};
inline const QColor Pad{200, 160, 90};
inline const QColor Copper{200, 160, 90};
}  // namespace boarddefaults

// パラメトリック基板定義。
//
// PasS から取り込んだ基板も含め、すべての基板はこの1つのモデルで表現する。
// PasS 由来の場合は padShape=None, copper=None にしたうえで元の BMP を
// background に埋め込み、格子・穴のジオメトリだけをパラメトリックに保持する。
// これによりスナップ・当たり判定・ネット算出は常に同じロジックで動く。
class BoardSpec {
public:
	QString id;
	QString name;
	QSize size;  // 基板外形 (単位系)

	// 格子
	int cols = 0;
	int rows = 0;
	int pitch = 10;
	QPoint origin;  // 外形左上から最初の穴中心までのオフセット (単位系)
	QVector<QPoint> absentHoles;  // 格子上で穴が無い位置 (col,row) — 変形基板対応

	// ランド
	PadShape padShape = PadShape::Round;
	int padDiameter = 6;
	int holeDiameter = 3;

	// 銅箔
	CopperPattern copper = CopperPattern::PadPerHole;
	QVector<QPoint> stripBreaks;  // ベロボードのランドカット位置 (col,row)
	bool doubleSided = false;

	// 見た目
	QColor substrateColor = boarddefaults::Substrate;
	QColor padColor = boarddefaults::Pad;
	QColor copperColor = boarddefaults::Copper;

	// 任意の装飾レイヤ (PasS 取込基板の元画像など)
	std::optional<Artwork> backgroundFront;
	std::optional<Artwork> backgroundBack;

	QVector<QPair<QPoint, int>> mountingHoles;  // 格子外の穴 (中心位置, 直径単位=drillコード)

	std::optional<QRect> outlineRect;  // PasS の幅マーカー由来。未設定 = 全面

	bool holeExistsAt(int col, int row) const;
	QPoint holeCenter(int col, int row) const {
		return origin + QPoint(col * pitch, row * pitch);
	}
	// 全穴の中心座標一覧 (absentHoles を除く)。
	QVector<QPoint> holeCenters() const;
	QRect effectiveOutline() const {
		return outlineRect.value_or(QRect(QPoint(0, 0), size));
	}
	// 単位座標に最も近い格子点の (col,row) を返す。範囲外でも clamp しない。
	QPoint nearestGridIndex(QPoint unitPos) const;
};
