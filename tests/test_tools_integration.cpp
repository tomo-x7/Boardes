#include <QImage>
#include <QTemporaryDir>
#include <QTest>

#include "io/partio.h"
#include "model/document.h"
#include "model/librarymanager.h"
#include "render/boardscene.h"
#include "render/boardview.h"
#include "render/items/placementitem.h"
#include "render/items/wireitem.h"
#include "ui/tools/snapengine.h"
#include "ui/tools/toolmanager.h"

// 実際の QGraphicsView に QTest::mouseClick/mousePress/mouseMove/mouseRelease で
// 本物のマウスイベントを送り込み、BoardView -> QGraphicsScene -> ToolManager -> Tool
// -> QUndoCommand -> Document という実際の経路が最初から最後まで正しく動くことを
// 検証する (ユニットテストでは見えない、イベント配送まわりの結線ミスを拾うため)。
class TestToolsIntegration : public QObject {
	Q_OBJECT

private slots:
	void init();
	void cleanup();

	void placePartToolAddsPlacementOnClick();
	void selectToolDragMovesPlacementWithUndo();
	void selectToolDeleteKeyRemovesSelection();
	void wireToolDrawsPolylineOnClicksAndFinishesOnDoubleClick();
	void rotateShortcutRotatesSelection();
	void hoveringWireHighlightsWholeNetButNotOtherNets();

private:
	std::unique_ptr<QTemporaryDir> m_appDataDir;
	std::unique_ptr<Document> m_doc;
	std::unique_ptr<LibraryManager> m_libMgr;
	std::unique_ptr<BoardScene> m_frontScene;
	std::unique_ptr<BoardScene> m_backScene;
	std::unique_ptr<BoardView> m_frontView;
	std::unique_ptr<ToolManager> m_toolManager;

	QString m_libId;
	QString m_partId;

	QPoint viewPosFor(QPoint sceneUnitPos) const;
};

void TestToolsIntegration::init() {
	m_doc = std::make_unique<Document>();
	m_doc->board.size = QSize(300, 300);
	m_doc->board.cols = 20;
	m_doc->board.rows = 20;
	m_doc->board.pitch = 10;
	m_doc->board.origin = QPoint(10, 10);

	// PlacePartTool は (実運用で壊れたライブラリ参照から配置してしまわないよう)
	// 部品が解決できないと配置を拒否する。そのためこのテストでも実際に
	// LibraryManager 経由でインポートした、解決可能な部品を使う必要がある。
	m_appDataDir = std::make_unique<QTemporaryDir>();
	QVERIFY(m_appDataDir->isValid());
	qputenv("XDG_DATA_HOME", m_appDataDir->path().toUtf8());  // 実ユーザー環境を汚さない
	m_libMgr = std::make_unique<LibraryManager>();
	m_libMgr->loadAll();

	Part part;
	part.id = "P1";
	part.name = "P1";
	part.refPrefix = "U";
	QImage img(20, 10, QImage::Format_RGB888);
	img.fill(Qt::blue);
	part.artwork = Artwork::fromImageAsIs(img);
	const QString bpartPath = m_appDataDir->filePath("P1.bpart");
	QVERIFY(partio::saveEmbedded(part, bpartPath));
	const auto importResult = m_libMgr->importPartFile(bpartPath);
	QVERIFY2(importResult.ok, qPrintable(importResult.error));
	m_libId = importResult.libraryId;  // マイライブラリの id
	m_partId = "P1";

	m_frontScene = std::make_unique<BoardScene>(Side::Front);
	m_backScene = std::make_unique<BoardScene>(Side::Back);
	m_frontScene->setDocument(m_doc.get(), m_libMgr.get());
	m_backScene->setDocument(m_doc.get(), m_libMgr.get());

	ToolContext ctx;
	ctx.document = m_doc.get();
	ctx.libraryManager = m_libMgr.get();
	ctx.frontScene = m_frontScene.get();
	ctx.backScene = m_backScene.get();
	static SnapEngine snapEngine;  // Granularity::Full が既定
	ctx.snapEngine = &snapEngine;
	m_toolManager = std::make_unique<ToolManager>(ctx);
	m_frontScene->setToolManager(m_toolManager.get());
	m_backScene->setToolManager(m_toolManager.get());

	m_frontView = std::make_unique<BoardView>();
	m_frontView->setScene(m_frontScene.get());
	m_frontView->resize(600, 600);
	m_frontView->show();
	QVERIFY(QTest::qWaitForWindowExposed(m_frontView.get()));
	// ビュー座標とシーン座標がずれないよう、変換を単位行列 (1シーン単位=1ビューpx)
	// 付近にしておく。fitBoardToWindow だとスケールが変わり計算が煩雑になるため
	// 使わない。
	m_frontView->resetZoom();
}

void TestToolsIntegration::cleanup() {
	m_frontView.reset();
	m_toolManager.reset();
	m_backScene.reset();
	m_frontScene.reset();
	m_libMgr.reset();
	m_doc.reset();
	m_appDataDir.reset();
}

QPoint TestToolsIntegration::viewPosFor(QPoint sceneUnitPos) const {
	return m_frontView->mapFromScene(QPointF(sceneUnitPos));
}

void TestToolsIntegration::placePartToolAddsPlacementOnClick() {
	m_toolManager->activatePlacePartTool(m_libId, m_partId);
	QCOMPARE(m_doc->placements.size(), 0);

	const QPoint clickAt = viewPosFor(QPoint(50, 50));
	QTest::mouseClick(m_frontView->viewport(), Qt::LeftButton, Qt::NoModifier, clickAt);

	QCOMPARE(m_doc->placements.size(), 1);
	// フルグリッド (10単位) にスナップされていること。
	QCOMPARE(m_doc->placements[0]->pos, QPoint(50, 50));
	QVERIFY(m_frontScene->placementItemFor(m_doc->placements[0]->uuid) != nullptr);

	// 続けてクリックするともう1つ置ける (連続配置モード)。
	const QPoint clickAt2 = viewPosFor(QPoint(80, 50));
	QTest::mouseClick(m_frontView->viewport(), Qt::LeftButton, Qt::NoModifier, clickAt2);
	QCOMPARE(m_doc->placements.size(), 2);
}

void TestToolsIntegration::selectToolDragMovesPlacementWithUndo() {
	auto placement = std::make_shared<Placement>();
	placement->uuid = "p1";
	placement->libraryId = m_libId;
	placement->partId = m_partId;
	placement->pos = QPoint(50, 50);
	m_doc->placements.append(placement);
	m_frontScene->syncPlacements();
	m_toolManager->activateSelectTool();

	const QPoint pressAt = viewPosFor(QPoint(55, 55));  // 部品の外形内 (20x10) をクリック
	const QPoint releaseAt = viewPosFor(QPoint(85, 85));  // +30,+30 だけドラッグ

	QTest::mousePress(m_frontView->viewport(), Qt::LeftButton, Qt::NoModifier, pressAt);
	QTest::mouseMove(m_frontView->viewport(), releaseAt);
	QTest::mouseRelease(m_frontView->viewport(), Qt::LeftButton, Qt::NoModifier, releaseAt);

	QCOMPARE(m_doc->placements[0]->pos, QPoint(80, 80));  // 30,30 移動 + フルグリッドスナップ

	m_doc->undoStack()->undo();
	QCOMPARE(m_doc->placements[0]->pos, QPoint(50, 50));

	m_doc->undoStack()->redo();
	QCOMPARE(m_doc->placements[0]->pos, QPoint(80, 80));
}

void TestToolsIntegration::selectToolDeleteKeyRemovesSelection() {
	auto placement = std::make_shared<Placement>();
	placement->uuid = "p1";
	placement->libraryId = m_libId;
	placement->partId = m_partId;
	placement->pos = QPoint(50, 50);
	m_doc->placements.append(placement);
	m_frontScene->syncPlacements();
	m_toolManager->activateSelectTool();

	const QPoint clickAt = viewPosFor(QPoint(55, 55));
	QTest::mouseClick(m_frontView->viewport(), Qt::LeftButton, Qt::NoModifier, clickAt);
	QVERIFY(m_frontScene->placementItemFor("p1")->isSelected());

	QTest::keyClick(m_frontView->viewport(), Qt::Key_Delete);
	QCOMPARE(m_doc->placements.size(), 0);

	m_doc->undoStack()->undo();
	QCOMPARE(m_doc->placements.size(), 1);
}

void TestToolsIntegration::wireToolDrawsPolylineOnClicksAndFinishesOnDoubleClick() {
	m_toolManager->activateWireTool(WireLayer::FrontBare);

	QTest::mouseClick(m_frontView->viewport(), Qt::LeftButton, Qt::NoModifier, viewPosFor(QPoint(20, 20)));
	QTest::mouseClick(m_frontView->viewport(), Qt::LeftButton, Qt::NoModifier, viewPosFor(QPoint(60, 20)));
	QCOMPARE(m_doc->wires.size(), 0);  // まだ確定していない

	QTest::mouseDClick(m_frontView->viewport(), Qt::LeftButton, Qt::NoModifier, viewPosFor(QPoint(60, 60)));
	QCOMPARE(m_doc->wires.size(), 1);
	QCOMPARE(m_doc->wires[0]->points.size(), 3);
	QCOMPARE(static_cast<int>(m_doc->wires[0]->layer), static_cast<int>(WireLayer::FrontBare));
	QVERIFY(m_frontScene->wireItemFor(m_doc->wires[0]->uuid) != nullptr);
}

void TestToolsIntegration::rotateShortcutRotatesSelection() {
	auto placement = std::make_shared<Placement>();
	placement->uuid = "p1";
	placement->libraryId = m_libId;
	placement->partId = m_partId;
	placement->pos = QPoint(50, 50);
	placement->rot = Rotation::R0;
	m_doc->placements.append(placement);
	m_frontScene->syncPlacements();
	m_toolManager->activateSelectTool();

	QTest::mouseClick(m_frontView->viewport(), Qt::LeftButton, Qt::NoModifier, viewPosFor(QPoint(55, 55)));
	QTest::keyClick(m_frontView->viewport(), Qt::Key_R);

	QCOMPARE(static_cast<int>(m_doc->placements[0]->rot), static_cast<int>(Rotation::R90));
	m_doc->undoStack()->undo();
	QCOMPARE(static_cast<int>(m_doc->placements[0]->rot), static_cast<int>(Rotation::R0));
}

void TestToolsIntegration::hoveringWireHighlightsWholeNetButNotOtherNets() {
	auto w1 = std::make_shared<Wire>();
	w1->uuid = "w1";
	w1->layer = WireLayer::FrontBare;
	w1->points = {QPoint(20, 20), QPoint(20, 40)};
	auto w2 = std::make_shared<Wire>();
	w2->uuid = "w2";
	w2->layer = WireLayer::FrontBare;
	w2->points = {QPoint(20, 40), QPoint(60, 40)};  // w1 の終点で接触 -> 同一ネット
	auto w3 = std::make_shared<Wire>();
	w3->uuid = "w3";
	w3->layer = WireLayer::FrontBare;
	w3->points = {QPoint(100, 100), QPoint(120, 100)};  // 別ネット
	m_doc->wires = {w1, w2, w3};
	m_frontScene->syncWires();
	m_toolManager->activateSelectTool();

	// w1 の中間点あたりにマウスを移動する (ボタンは押さない、純粋なホバー)。
	QTest::mouseMove(m_frontView->viewport(), viewPosFor(QPoint(20, 30)));

	QVERIFY(m_frontScene->wireItemFor("w1")->isNetHighlighted());
	QVERIFY(m_frontScene->wireItemFor("w2")->isNetHighlighted());
	QVERIFY(!m_frontScene->wireItemFor("w3")->isNetHighlighted());

	// 何もない場所に移動するとハイライトが消える。
	QTest::mouseMove(m_frontView->viewport(), viewPosFor(QPoint(200, 200)));
	QVERIFY(!m_frontScene->wireItemFor("w1")->isNetHighlighted());
	QVERIFY(!m_frontScene->wireItemFor("w2")->isNetHighlighted());
}

QTEST_MAIN(TestToolsIntegration)
#include "test_tools_integration.moc"
