#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

#include "io/imageexport.h"
#include "model/document.h"
#include "render/boardscene.h"

class TestImageExport : public QObject {
	Q_OBJECT

private slots:
	void init();
	void cleanup();

	void pngRespectsSceneSizeAndScale();
	void pngBothSidesIsWiderThanEitherSide();
	void transparentBackgroundLeavesCornerPixelTransparent();
	void opaqueBackgroundFillsCornerWhite();
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

void TestImageExport::transparentBackgroundLeavesCornerPixelTransparent() {
	// 基板自体は不透明に塗りつぶされるため、基板の外側 (シーン矩形の余白部分) で
	// 検証する。余白を持たせるため sceneRect を基板より広く取る。
	m_front->setSceneRect(-20, -20, 140, 100);
	imageexport::Options options;
	options.transparentBackground = true;
	const QImage img = imageexport::renderToImage(m_front.get(), options);
	QCOMPARE(qAlpha(img.pixel(0, 0)), 0);
}

void TestImageExport::opaqueBackgroundFillsCornerWhite() {
	m_front->setSceneRect(-20, -20, 140, 100);
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

QTEST_MAIN(TestImageExport)
#include "test_imageexport.moc"
