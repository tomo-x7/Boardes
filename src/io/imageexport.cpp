#include "imageexport.h"

#include <QFileInfo>
#include <QGraphicsItem>
#include <QPainter>
#include <QSvgGenerator>
#include <memory>

#include "../render/boardscene.h"

namespace imageexport {

namespace {

// エクスポート中だけ選択状態を一時的に解除し、終了後に元へ戻す
// (選択中のアイテムの黄色いハイライトが書き出し画像に写り込まないようにするため)。
class SelectionGuard {
public:
	explicit SelectionGuard(BoardScene *scene) : m_scene(scene) {
		if (m_scene) {
			m_previouslySelected = m_scene->selectedItems();
			m_scene->clearSelection();
		}
	}
	~SelectionGuard() {
		if (m_scene) {
			for (auto *item : std::as_const(m_previouslySelected)) {
				item->setSelected(true);
			}
		}
	}

private:
	BoardScene *m_scene;
	QList<QGraphicsItem *> m_previouslySelected;
};

}  // namespace

QImage renderToImage(BoardScene *scene, const Options &options) {
	if (!scene) {
		return {};
	}
	SelectionGuard guard(scene);

	// sceneRect() はパン用の余白を含む (BoardScene::syncBoard() 参照) ので、書き出しには
	// 余白なしの基板外形 (boardRect()) を使う。
	const QRectF exportRect = scene->boardRect();
	const QSize sizePx(qMax(1, qRound(exportRect.width() * options.scale)),
					   qMax(1, qRound(exportRect.height() * options.scale)));

	QImage img(sizePx, QImage::Format_ARGB32_Premultiplied);
	img.fill(options.transparentBackground ? Qt::transparent : Qt::white);

	QPainter painter(&img);
	painter.setRenderHint(QPainter::Antialiasing, true);
	painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
	scene->render(&painter, QRectF(QPointF(0, 0), QSizeF(sizePx)), exportRect);
	painter.end();
	return img;
}

QImage renderBothSides(BoardScene *front, BoardScene *back, const Options &options) {
	const QImage frontImg = renderToImage(front, options);
	const QImage backImg = renderToImage(back, options);
	if (frontImg.isNull()) {
		return backImg;
	}
	if (backImg.isNull()) {
		return frontImg;
	}

	const int gap = 8 * options.scale;
	const int w = frontImg.width() + gap + backImg.width();
	const int h = qMax(frontImg.height(), backImg.height());
	QImage combined(w, h, QImage::Format_ARGB32_Premultiplied);
	combined.fill(options.transparentBackground ? Qt::transparent : Qt::white);

	QPainter p(&combined);
	p.drawImage(0, 0, frontImg);
	p.drawImage(frontImg.width() + gap, 0, backImg);
	p.end();
	return combined;
}

QImage render(BoardScene *front, BoardScene *back, const Options &options) {
	switch (options.target) {
	case Target::Front:
		return renderToImage(front, options);
	case Target::Back:
		return renderToImage(back, options);
	case Target::Both:
		return renderBothSides(front, back, options);
	}
	return {};
}

bool saveAsPng(BoardScene *front, BoardScene *back, const Options &options, const QString &filePath) {
	const QImage img = render(front, back, options);
	if (img.isNull()) {
		return false;
	}
	return img.save(filePath, "PNG");
}

bool saveAsSvg(BoardScene *front, BoardScene *back, const Options &options, const QString &filePath) {
	const bool needFront = options.target == Target::Front || options.target == Target::Both;
	const bool needBack = options.target == Target::Back || options.target == Target::Both;
	if ((needFront && !front) || (needBack && !back)) {
		return false;
	}

	const QRectF frontRect = front ? front->boardRect() : QRectF();
	const QRectF backRect = back ? back->boardRect() : QRectF();
	constexpr qreal kGap = 8.0;

	QSizeF logicalSize;
	switch (options.target) {
	case Target::Front:
		logicalSize = frontRect.size();
		break;
	case Target::Back:
		logicalSize = backRect.size();
		break;
	case Target::Both:
		logicalSize = QSizeF(frontRect.width() + kGap + backRect.width(), qMax(frontRect.height(), backRect.height()));
		break;
	}
	if (logicalSize.isEmpty()) {
		return false;
	}

	const QSize sizePx(qMax(1, qRound(logicalSize.width() * options.scale)),
					   qMax(1, qRound(logicalSize.height() * options.scale)));

	QSvgGenerator generator;
	generator.setFileName(filePath);
	generator.setSize(sizePx);
	generator.setViewBox(QRect(QPoint(0, 0), sizePx));
	generator.setTitle(QStringLiteral("Boardes"));

	{
		QPainter painter(&generator);
		painter.setRenderHint(QPainter::Antialiasing, true);
		painter.scale(options.scale, options.scale);
		if (!options.transparentBackground) {
			painter.fillRect(QRectF(QPointF(0, 0), logicalSize), Qt::white);
		}

		std::unique_ptr<SelectionGuard> frontGuard, backGuard;
		if (needFront) frontGuard = std::make_unique<SelectionGuard>(front);
		if (needBack) backGuard = std::make_unique<SelectionGuard>(back);

		if (needFront) {
			front->render(&painter, QRectF(QPointF(0, 0), frontRect.size()), frontRect);
		}
		if (needBack) {
			const qreal xOffset = (options.target == Target::Both) ? (frontRect.width() + kGap) : 0.0;
			back->render(&painter, QRectF(QPointF(xOffset, 0), backRect.size()), backRect);
		}
	}  // painter を generator より先に破棄して確実にフラッシュさせる

	return QFileInfo::exists(filePath) && QFileInfo(filePath).size() > 0;
}

}  // namespace imageexport
