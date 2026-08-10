#include "icons.h"

#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>

namespace icons {

// 各アイコンの中身は claude.ai/design「Boardes UI」プロジェクトの
// screens/redesign-main-window-light.html に実際に描画されているインラインSVGから
// そのまま転記したもの (viewBox 0 0 24 24 前提)。`currentColor` はレンダリング時に
// renderSvg() が指定色へ置換する。

const IconSpec ZoomIn{
	"<circle cx='11' cy='11' r='7'/><line x1='11' y1='8' x2='11' y2='14'/>"
	"<line x1='8' y1='11' x2='14' y2='11'/><line x1='16.2' y1='16.2' x2='21' y2='21'/>"};
const IconSpec ZoomOut{
	"<circle cx='11' cy='11' r='7'/><line x1='8' y1='11' x2='14' y2='11'/>"
	"<line x1='16.2' y1='16.2' x2='21' y2='21'/>"};
const IconSpec Fit{"<path d='M4 9V4h5M20 9V4h-5M4 15v5h5M20 15v5h-5'/>"};
const IconSpec Orientation{
	"<line x1='12' y1='3' x2='12' y2='21' stroke-dasharray='2 2'/>"
	"<path d='M7 8l-3 4 3 4M17 8l3 4-3 4'/>"};
const IconSpec LinkViews{"<circle cx='8' cy='12' r='4'/><circle cx='16' cy='12' r='4'/>"};
const IconSpec WireDot{
	"<circle cx='5' cy='19' r='1.6' fill='currentColor' stroke='none'/>"
	"<circle cx='19' cy='5' r='1.6' fill='currentColor' stroke='none'/>"
	"<line x1='5' y1='19' x2='19' y2='5'/>"};
const IconSpec WireDotThick{
	"<circle cx='5' cy='19' r='1.8' fill='currentColor' stroke='none'/>"
	"<circle cx='19' cy='5' r='1.8' fill='currentColor' stroke='none'/>"
	"<line x1='5' y1='19' x2='19' y2='5' stroke-width='3.4'/>"};
const IconSpec Outline{"<rect x='4' y='4' width='16' height='16' rx='1' stroke-dasharray='3 2.5'/>"};
const IconSpec PartOutline{"<rect x='4' y='4' width='16' height='16' rx='1'/><rect x='8' y='8' width='8' height='8' rx='0.5'/>"};
const IconSpec PinNumbers{
	"<circle cx='7' cy='7' r='2.4'/><circle cx='17' cy='7' r='2.4'/>"
	"<circle cx='7' cy='17' r='2.4'/><circle cx='17' cy='17' r='2.4'/>"};
const IconSpec Labels{"<rect x='4' y='8' width='16' height='8' rx='1.5'/><line x1='8' y1='12' x2='16' y2='12'/>"};
const IconSpec PinMarkers{"<circle cx='12' cy='12' r='7'/><circle cx='12' cy='12' r='1.6' fill='currentColor' stroke='none'/>"};

const IconSpec Select{"<path d='M6 4l12 8-5.2.6L16 19l-2.6 1.2-3-6.6L6 17V4z' stroke-linejoin='round'/>"};
const IconSpec DrawOutline{
	"<path d='M5 19V7a2 2 0 0 1 2-2h9'/><path d='M15 4l4 4-8.5 8.5H7v-3.5z' stroke-linejoin='round'/>"};
const IconSpec Draft{
	"<line x1='5' y1='19' x2='16' y2='8' stroke-dasharray='2.5 2'/>"
	"<path d='M16 8l2.5-2.5 2 2L18 10z' stroke-linejoin='round'/>"};

const IconSpec ZoomBarMinus{"<line x1='5' y1='12' x2='19' y2='12'/>"};
const IconSpec ZoomBarPlus{"<line x1='12' y1='5' x2='12' y2='19'/><line x1='5' y1='12' x2='19' y2='12'/>"};

namespace {

// 論理サイズ logicalSize (px) ・拡大率 dpr でアイコンを1枚ラスタライズする。
QPixmap renderPixmap(const IconSpec &spec, const QColor &color, qreal opacity, int logicalSize, qreal dpr) {
	QString body = QString::fromUtf8(spec.body);
	body.replace(QStringLiteral("currentColor"), color.name());
	const QString svg = QStringLiteral(
							 "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' "
							 "stroke='%1' stroke-width='%2' stroke-linecap='round' stroke-linejoin='round'>%3</svg>")
							 .arg(color.name(), QString::number(spec.strokeWidth), body);

	QSvgRenderer renderer(svg.toUtf8());
	const int px = qRound(logicalSize * dpr);
	QPixmap pm(px, px);
	pm.fill(Qt::transparent);
	{
		QPainter painter(&pm);
		painter.setRenderHint(QPainter::Antialiasing);
		painter.setOpacity(opacity);
		renderer.render(&painter, QRectF(0, 0, px, px));
	}
	pm.setDevicePixelRatio(dpr);
	return pm;
}

// 通常解像度 (14px) と高DPI用 (28px, dpr=2) の2枚を addPixmap したアイコンを作る。
void addBothResolutions(QIcon &icon, const IconSpec &spec, const QColor &color, qreal opacity, QIcon::Mode mode,
						 QIcon::State state) {
	icon.addPixmap(renderPixmap(spec, color, opacity, 14, 1.0), mode, state);
	icon.addPixmap(renderPixmap(spec, color, opacity, 14, 2.0), mode, state);
}

}  // namespace

QIcon lineIcon(const IconSpec &spec, const QColor &normalColor, const QColor &activeColor) {
	QIcon icon;
	addBothResolutions(icon, spec, normalColor, 1.0, QIcon::Normal, QIcon::Off);
	addBothResolutions(icon, spec, activeColor, 1.0, QIcon::Normal, QIcon::On);
	return icon;
}

QIcon wireLineIcon(const IconSpec &spec, const QColor &color) {
	QIcon icon;
	addBothResolutions(icon, spec, color, 0.5, QIcon::Normal, QIcon::Off);
	addBothResolutions(icon, spec, color, 1.0, QIcon::Normal, QIcon::On);
	return icon;
}

QIcon soloIcon(const IconSpec &spec, const QColor &color) {
	QIcon icon;
	addBothResolutions(icon, spec, color, 1.0, QIcon::Normal, QIcon::Off);
	addBothResolutions(icon, spec, color, 1.0, QIcon::Normal, QIcon::On);
	return icon;
}

QIcon colorSwatch(const QColor &color, int sizePx) {
	QIcon icon;
	for (const qreal dpr : {1.0, 2.0}) {
		const int px = qRound(sizePx * dpr);
		QPixmap pm(px, px);
		pm.fill(Qt::transparent);
		QPainter painter(&pm);
		painter.setPen(Qt::NoPen);
		painter.setBrush(color);
		painter.drawRoundedRect(QRectF(0, 0, px, px), 1 * dpr, 1 * dpr);
		painter.end();
		pm.setDevicePixelRatio(dpr);
		icon.addPixmap(pm);
	}
	return icon;
}

}  // namespace icons
