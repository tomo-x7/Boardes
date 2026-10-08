#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QTemporaryDir>
#include <QTest>
#include <tuple>

#include "io/imageexport.h"
#include "model/document.h"
#include "render/boardscene.h"
#include "render/items/overlayitem.h"
#include "render/items/wireitem.h"

class TestImageExport : public QObject {
	Q_OBJECT

private slots:
	void init();
	void cleanup();

	void pngRespectsSceneSizeAndScale();
	void pngBothSidesIsWiderThanEitherSide();
	void transparentBackgroundLeavesCornerPixelTransparent();
	void opaqueBackgroundFillsCornerWhite();
	void overlayIsNotDrawnInPng();
	void overlayIsNotWrittenToSvg();
	void overlayIsShownAgainAfterExport();
	void netHighlightIsNotDrawnInExport();
	void svgFileIsCreatedForEachTarget();

private:
	std::unique_ptr<Document> m_doc;
	std::unique_ptr<BoardScene> m_front;
	std::unique_ptr<BoardScene> m_back;
};

void TestImageExport::init() {
	m_doc = std::make_unique<Document>();
	m_doc->board.size = QSize(100, 60);
	m_front = std::make_unique<BoardScene>(Side::Front);
	m_back = std::make_unique<BoardScene>(Side::Back);
	m_front->setDocument(m_doc.get(), nullptr);
	m_back->setDocument(m_doc.get(), nullptr);
}

void TestImageExport::cleanup() {
	m_back.reset();
	m_front.reset();
	m_doc.reset();
}

void TestImageExport::pngRespectsSceneSizeAndScale() {
	imageexport::Options options;
	options.target = imageexport::Target::Front;
	options.scale = 1;
	const QImage img1x = imageexport::renderToImage(m_front.get(), options);
	QCOMPARE(img1x.width(), 100);
	QCOMPARE(img1x.height(), 60);

	options.scale = 4;
	const QImage img4x = imageexport::renderToImage(m_front.get(), options);
	QCOMPARE(img4x.width(), 400);
	QCOMPARE(img4x.height(), 240);
}

void TestImageExport::pngBothSidesIsWiderThanEitherSide() {
	imageexport::Options options;
	options.target = imageexport::Target::Both;
	const QImage combined = imageexport::render(m_front.get(), m_back.get(), options);
	QVERIFY(combined.width() > 100 * 2);  // 隙間分だけ2枚より広い
	QCOMPARE(combined.height(), 60);
}

namespace {
// 書き出し画像は boardRect() ぴったりにトリミングされる (Phase 12) ので、基板の外側の
// 余白では検証できない。そのかわり、完全に透過な背景画像を基板に貼ることで
// 「基板自体は何も塗らない」状況を作り、options.transparentBackground が effective に
// なる (initial QImage::fill() がそのまま残る) ことを検証する。
void useFullyTransparentBoard(Document &doc) {
	doc.board.size = QSize(10, 10);
	QImage bg(10, 10, QImage::Format_ARGB32);
	bg.fill(Qt::transparent);
	doc.board.backgroundFront = Artwork::fromImageAsIs(bg);
}

// 下書きの線 (確定済みと描いている最中)・入力中の配線のプレビュー・部品配置のゴーストを、
// 基板 (useFullyTransparentBoard で 10x10) いっぱいに載せる。
void addOverlayContent(BoardScene &scene) {
	OverlayItem *overlay = scene.overlay();
	overlay->addDraftStroke({QPointF(1, 1), QPointF(9, 9)});
	overlay->setLiveDraftStroke({QPointF(1, 9), QPointF(9, 1)});
	overlay->setWirePreview({QPoint(1, 5), QPoint(9, 5)}, QPoint(9, 9), WireLayer::FrontBare);
	QImage ghost(6, 6, QImage::Format_ARGB32);
	ghost.fill(Qt::blue);
	overlay->setPlacementGhost(ghost, QPoint(2, 2), QSize(6, 6), true);
}

int countPixelsDifferentFrom(const QImage &img, QColor color) {
	int count = 0;
	for (int y = 0; y < img.height(); ++y) {
		for (int x = 0; x < img.width(); ++x) {
			if (img.pixelColor(x, y) != color) {
				++count;
			}
		}
	}
	return count;
}

QByteArray readFile(const QString &path) {
	QFile file(path);
	return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

// エクスポートを通さず、シーンの基板全体をそのまま描く。
QImage renderDirectly(BoardScene &scene) {
	QImage img(scene.boardRect().size().toSize(), QImage::Format_ARGB32_Premultiplied);
	img.fill(Qt::transparent);
	QPainter painter(&img);
	scene.render(&painter, QRectF(QPointF(0, 0), QSizeF(img.size())), scene.boardRect());
	painter.end();
	return img;
}
}  // namespace

void TestImageExport::transparentBackgroundLeavesCornerPixelTransparent() {
	useFullyTransparentBoard(*m_doc);
	m_front->syncBoard();
	imageexport::Options options;
	options.transparentBackground = true;
	const QImage img = imageexport::renderToImage(m_front.get(), options);
	QCOMPARE(qAlpha(img.pixel(0, 0)), 0);
}

void TestImageExport::opaqueBackgroundFillsCornerWhite() {
	useFullyTransparentBoard(*m_doc);
	m_front->syncBoard();
	imageexport::Options options;
	options.transparentBackground = false;
	const QImage img = imageexport::renderToImage(m_front.get(), options);
	QCOMPARE(img.pixelColor(0, 0), QColor(Qt::white));
}

void TestImageExport::svgFileIsCreatedForEachTarget() {
	QTemporaryDir dir;
	QVERIFY(dir.isValid());

	for (auto target : {imageexport::Target::Front, imageexport::Target::Back, imageexport::Target::Both}) {
		imageexport::Options options;
		options.target = target;
		const QString path = dir.filePath(QStringLiteral("out_%1.svg").arg(static_cast<int>(target)));
		QVERIFY(imageexport::saveAsSvg(m_front.get(), m_back.get(), options, path));
		QVERIFY(QFileInfo::exists(path));
		QVERIFY(QFileInfo(path).size() > 0);
	}
}

void TestImageExport::overlayIsNotDrawnInPng() {
	// 基板も背景も何も塗らない状態にして、オーバーレイの内容だけを載せる。
	useFullyTransparentBoard(*m_doc);
	m_doc->board.backgroundBack = m_doc->board.backgroundFront;
	m_front->syncBoard();
	m_back->syncBoard();
	addOverlayContent(*m_front);
	addOverlayContent(*m_back);

	// 前提: オーバーレイが見えている状態でそのままシーンを描くと、内容が写る。
	QImage direct(10, 10, QImage::Format_ARGB32_Premultiplied);
	direct.fill(Qt::transparent);
	{
		QPainter painter(&direct);
		m_front->render(&painter, QRectF(0, 0, 10, 10), m_front->boardRect());
	}
	QVERIFY(countPixelsDifferentFrom(direct, Qt::transparent) > 0);

	// 書き出した画像には、下書き・配線のプレビュー・ゴーストが一切写らない。
	for (const bool transparentBackground : {true, false}) {
		for (auto target : {imageexport::Target::Front, imageexport::Target::Back, imageexport::Target::Both}) {
			imageexport::Options options;
			options.target = target;
			options.transparentBackground = transparentBackground;
			const QImage img = imageexport::render(m_front.get(), m_back.get(), options);
			QVERIFY(!img.isNull());
			QCOMPARE(countPixelsDifferentFrom(img, transparentBackground ? QColor(Qt::transparent) : QColor(Qt::white)),
					 0);
		}
	}
}

void TestImageExport::overlayIsNotWrittenToSvg() {
	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	imageexport::Options options;
	options.target = imageexport::Target::Both;

	// オーバーレイに何も無いときの SVG を基準にする。内容を載せても、SVG は変わらない。
	const QString plainPath = dir.filePath(QStringLiteral("plain.svg"));
	QVERIFY(imageexport::saveAsSvg(m_front.get(), m_back.get(), options, plainPath));
	addOverlayContent(*m_front);
	addOverlayContent(*m_back);
	const QString path = dir.filePath(QStringLiteral("with-overlay.svg"));
	QVERIFY(imageexport::saveAsSvg(m_front.get(), m_back.get(), options, path));

	const QByteArray svg = readFile(path);
	QVERIFY(!svg.isEmpty());
	QVERIFY2(!svg.contains("#5050c8"), "下書きの線の色が書き出されている");
	QVERIFY2(!svg.contains("#2060d0"), "配線のプレビューの色が書き出されている");
	QCOMPARE(svg, readFile(plainPath));
}

void TestImageExport::overlayIsShownAgainAfterExport() {
	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	addOverlayContent(*m_front);
	addOverlayContent(*m_back);
	imageexport::Options options;
	options.target = imageexport::Target::Both;

	// 書き出しが終わったら、オーバーレイは表示に戻り、内容も消えていない (画面に出ていた下書きや
	// 入力中の配線が、書き出しのせいで消えてはいけない)。
	QVERIFY(!imageexport::render(m_front.get(), m_back.get(), options).isNull());
	QVERIFY(imageexport::saveAsSvg(m_front.get(), m_back.get(), options, dir.filePath(QStringLiteral("out.svg"))));
	for (BoardScene *scene : {m_front.get(), m_back.get()}) {
		QVERIFY(scene->overlay()->isVisible());
		QVERIFY(scene->hasDraft());
		QVERIFY(!scene->overlay()->boundingRect().isEmpty());
	}

	// 元から隠れていたオーバーレイは、書き出しのあとも隠れたまま (勝手に表示しない)。
	m_front->overlay()->hide();
	QVERIFY(!imageexport::render(m_front.get(), m_back.get(), options).isNull());
	QVERIFY(imageexport::saveAsSvg(m_front.get(), m_back.get(), options, dir.filePath(QStringLiteral("out2.svg"))));
	QVERIFY(!m_front->overlay()->isVisible());
	QVERIFY(m_back->overlay()->isVisible());
}

void TestImageExport::netHighlightIsNotDrawnInExport() {
	// 基板も背景も何も塗らない状態にして、配線だけを載せる。wf (表) と wb (裏) はこのあと同一ネットの
	// 強調を付け、wn (表) は付けない。
	useFullyTransparentBoard(*m_doc);
	m_doc->board.backgroundBack = m_doc->board.backgroundFront;
	const QVector<std::tuple<QString, WireLayer, int>> wires = {
		{QStringLiteral("wf"), WireLayer::FrontBare, 3},
		{QStringLiteral("wn"), WireLayer::FrontBare, 7},
		{QStringLiteral("wb"), WireLayer::BackBare, 5},
	};
	for (const auto &[uuid, layer, y] : wires) {
		auto wire = std::make_shared<Wire>();
		wire->uuid = uuid;
		wire->layer = layer;
		wire->points = {QPoint(1, y), QPoint(9, y)};
		m_doc->wires.append(wire);
	}
	m_front->syncBoard();
	m_back->syncBoard();
	m_front->syncWires();
	m_back->syncWires();

	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	const auto exportAll = [&](const QString &svgName) {
		QList<QImage> images;
		imageexport::Options options;
		options.transparentBackground = true;
		for (auto target : {imageexport::Target::Front, imageexport::Target::Back, imageexport::Target::Both}) {
			options.target = target;
			images.append(imageexport::render(m_front.get(), m_back.get(), options));
		}
		options.target = imageexport::Target::Both;
		const bool saved = imageexport::saveAsSvg(m_front.get(), m_back.get(), options, dir.filePath(svgName));
		return std::make_pair(images, saved);
	};
	const auto [plain, plainSaved] = exportAll(QStringLiteral("plain.svg"));
	QVERIFY(plainSaved);
	QVERIFY(countPixelsDifferentFrom(plain[0], Qt::transparent) > 0);  // 前提: 配線は写っている

	// 前提: 強調が付いたシーンをそのまま描くと、縁取りが写って見た目が変わる。
	m_front->wireItemFor("wf")->setNetHighlighted(true);
	m_back->wireItemFor("wb")->setNetHighlighted(true);
	QVERIFY(renderDirectly(*m_front) != plain[0]);

	// 強調が付いていても、書き出した画像・SVG は付いていないときと同じ。
	const auto [highlighted, highlightedSaved] = exportAll(QStringLiteral("highlighted.svg"));
	QVERIFY(highlightedSaved);
	for (int i = 0; i < plain.size(); ++i) {
		QVERIFY2(highlighted[i] == plain[i], qPrintable(QString("target %1 に強調が写っている").arg(i)));
	}
	const QByteArray svg = readFile(dir.filePath(QStringLiteral("highlighted.svg")));
	QVERIFY(!svg.isEmpty());
	QVERIFY2(!svg.contains("#00dcdc"), "強調の縁取りの色が書き出されている");
	QCOMPARE(svg, readFile(dir.filePath(QStringLiteral("plain.svg"))));

	// 書き出しが終わったら、強調していた配線だけが元どおり強調に戻る (付いていなかった配線には付けない)。
	QVERIFY(m_front->wireItemFor("wf")->isNetHighlighted());
	QVERIFY(m_back->wireItemFor("wb")->isNetHighlighted());
	QVERIFY(!m_front->wireItemFor("wn")->isNetHighlighted());
}

QTEST_MAIN(TestImageExport)
#include "test_imageexport.moc"
