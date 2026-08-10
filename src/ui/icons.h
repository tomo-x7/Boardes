#pragma once

#include <QColor>
#include <QIcon>

// 表示/ツールツールバー・ステータスバー等の線画アイコン。Phase 20 (claude.ai/design 連携の
// UI 刷新)。claude.ai/design「Boardes UI」プロジェクトのモックアップ
// (screens/redesign-main-window-*.html) に実際に描画されているインラインSVGのパスを
// そのまま transcribe した定数を使う。単色ストローク線画、viewBox 0 0 24 24、
// 線幅1.5・線端丸・線結合丸 (仕様書 §6)。
namespace icons {

// 1個のアイコンの内側マークアップ (root の <svg> タグ自体は含まない)。
// `currentColor` はレンダリング時に指定色へ置換する (円の塗り等に使われている)。
struct IconSpec {
	const char *body;
	qreal strokeWidth = 1.5;
};

// ---- 表示ツールバー (1段目) ----
extern const IconSpec ZoomIn;
extern const IconSpec ZoomOut;
extern const IconSpec Fit;
extern const IconSpec Orientation;
extern const IconSpec LinkViews;
extern const IconSpec WireDot;       // 配線 (表面配線/裏面配線、素の配線ツールでも共用)
extern const IconSpec WireDotThick;  // 被覆配線 (表面/裏面、素の被覆配線ツールでも共用)
extern const IconSpec Outline;
extern const IconSpec PartOutline;
extern const IconSpec PinNumbers;
extern const IconSpec Labels;
extern const IconSpec PinMarkers;

// ---- ツールツールバー (2段目) ----
extern const IconSpec Select;
extern const IconSpec DrawOutline;
extern const IconSpec Draft;

// ---- ステータスバーの倍率バー ----
extern const IconSpec ZoomBarMinus;
extern const IconSpec ZoomBarPlus;

// 通常色/強調色 (チェック状態) の2枚のピクスマップを持つ QIcon を作る。
// QIcon::Normal/Off = normalColor、QIcon::Normal/On = activeColor。
QIcon lineIcon(const IconSpec &spec, const QColor &normalColor, const QColor &activeColor);

// 固定色 (配線色) のみで、Off=50%不透明・On=100%不透明の2枚を持つ QIcon を作る。
// 表示ツールバーの配線系トグル (レイヤ表示/非表示) 用。
QIcon wireLineIcon(const IconSpec &spec, const QColor &color);

// 単色・常に不透明の QIcon (Off/On とも同じ絵)。ツールツールバーの操作ボタン等、
// チェック状態の見た目を下線インジケータ (toolAction プロパティ) 側に任せる場合に使う。
QIcon soloIcon(const IconSpec &spec, const QColor &color);

// 単色の正方形スウォッチアイコン (接点マーカーの色ボタン等、色そのものを見せたい箇所用)。
QIcon colorSwatch(const QColor &color, int sizePx = 13);

}  // namespace icons
