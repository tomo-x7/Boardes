#include <QImage>
#include <QMouseEvent>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QTest>

#include "io/partio.h"
#include "model/document.h"
#include "model/librarymanager.h"
#include "render/boardscene.h"
#include "render/boardview.h"
#include "render/items/overlayitem.h"
#include "render/items/placementitem.h"
#include "render/items/wireitem.h"
#include "ui/mainwindow.h"
#include "ui/tools/drafttool.h"
#include "ui/tools/placeparttool.h"
#include "ui/tools/selecttool.h"
#include "ui/tools/snapengine.h"
#include "ui/tools/toolmanager.h"
#include "ui/tools/wiretool.h"

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
	void wireToolDrawsPolylineOnClicksAndFinishesOnRightClick();
	void wireToolEscDiscardsInProgressWire();
	void wireToolIgnoresClickOnSameVertexAgain();
	void wireToolCountsSecondFastClickAsVertex();
	void wireToolOverlayBoundsCoverConfirmedPointsAndCursor();
	void wireToolHidesRubberSegmentWhenMouseLeavesView();
	void selectToolStillHitsPartAndWireUnderDraftStroke();
	void draftStrokeIsCommittedWhenReleasedWithDifferentModifiers();
	void draftStrokeIgnoresMovesWithoutTheButtonHeld();
	void spaceHeldLeftDragPansInsteadOfDrawingWire();
	void spaceHeldDoubleClickAlsoPansInsteadOfDrawingWire();
	void spaceHeldIsClearedWhenViewLosesFocus();
	void middleButtonPanEndsEvenIfModifiersChangeBeforeRelease();
	void rotateShortcutRotatesSelection();
	void hoveringWireHighlightsWholeNetButNotOtherNets();
	void escapeReturnsPlacePartToolToSelectTool();
	void escapeAfterWireCommitChecksSelectToolButton();
	void toolbarButtonsFollowTheActiveTool();
	void backSceneCoordinatesAreNotMirrored();

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
	QPoint scrollPosition() const;
	void dragMoveTo(QPoint viewPos, Qt::MouseButton heldButton = Qt::LeftButton);
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
	// ピン1を原点 (0,0) に置く。これにより resolveAnchor() が (0,0) を返すので、
	// 配置位置 (Placement::pos) はそのままスナップ後の格子点と一致する
	// (このテストの各アサーションは「部品原点=クリック位置」という前提で書かれている)。
	Pin pin;
	pin.number = 1;
	pin.pos = QPoint(0, 0);
	part.pins = {pin};
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
	snapEngine.setBoard(&m_doc->board);
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
	// offscreen QPA では、直前のテストが破棄したウィンドウの最後のグローバル座標と
	// 今回の移動先がたまたま一致すると、QTest::mouseMove() が「移動なし」とみなして
	// イベントを送らないことがある (各テストが同じサイズ・同じ位置にウィンドウを
	// 作り直すため起こりやすい)。原点付近へのダミー移動を挟んで、この後の本番の
	// mouseMove が必ず「実際に動いた」と認識されるようにしておく。
	QTest::mouseMove(m_frontView->viewport(), QPoint(1, 1));
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

QPoint TestToolsIntegration::scrollPosition() const {
	return QPoint(m_frontView->horizontalScrollBar()->value(), m_frontView->verticalScrollBar()->value());
}

// ボタンを押したままのマウス移動を送る。QTest::mouseMove() はボタンを押していない移動に
// なるので、ドラッグ中の移動はこちらで送る。
void TestToolsIntegration::dragMoveTo(QPoint viewPos, Qt::MouseButton heldButton) {
	QWidget *viewport = m_frontView->viewport();
	QMouseEvent move(QEvent::MouseMove, QPointF(viewPos), QPointF(viewport->mapToGlobal(viewPos)), Qt::NoButton,
					 heldButton, Qt::NoModifier);
	QApplication::sendEvent(viewport, &move);
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

void TestToolsIntegration::wireToolDrawsPolylineOnClicksAndFinishesOnRightClick() {
	m_toolManager->activateWireTool(WireKind::Bare);

	// 左クリックは頂点を追加するだけ。右クリック (またはダブルクリックではなく Enter) は
	// それまでに追加済みの頂点で確定するだけで、右クリック位置そのものを新たな頂点として
	// 追加はしない (13-3)。3頂点で確定したいなら3回左クリックしてから右クリックする。
	QTest::mouseClick(m_frontView->viewport(), Qt::LeftButton, Qt::NoModifier, viewPosFor(QPoint(20, 20)));
	QTest::mouseClick(m_frontView->viewport(), Qt::LeftButton, Qt::NoModifier, viewPosFor(QPoint(60, 20)));
	QTest::mouseClick(m_frontView->viewport(), Qt::LeftButton, Qt::NoModifier, viewPosFor(QPoint(60, 60)));
	QCOMPARE(m_doc->wires.size(), 0);  // まだ確定していない

	QTest::mouseClick(m_frontView->viewport(), Qt::RightButton, Qt::NoModifier, viewPosFor(QPoint(60, 60)));
	QCOMPARE(m_doc->wires.size(), 1);
	QCOMPARE(m_doc->wires[0]->points.size(), 3);
	QCOMPARE(static_cast<int>(m_doc->wires[0]->layer), static_cast<int>(WireLayer::FrontBare));
	QVERIFY(m_frontScene->wireItemFor(m_doc->wires[0]->uuid) != nullptr);
}

void TestToolsIntegration::wireToolEscDiscardsInProgressWire() {
	m_toolManager->activateWireTool(WireKind::Bare);

	QTest::mouseClick(m_frontView->viewport(), Qt::LeftButton, Qt::NoModifier, viewPosFor(QPoint(20, 20)));
	QTest::mouseClick(m_frontView->viewport(), Qt::LeftButton, Qt::NoModifier, viewPosFor(QPoint(60, 20)));

	// Esc は「作図中の破棄」を優先する。ツール自体はまだ WireTool のまま。
	m_toolManager->cancelCurrent();
	QCOMPARE(m_doc->wires.size(), 0);
	QVERIFY(dynamic_cast<WireTool *>(m_toolManager->activeTool()) != nullptr);

	// 何も作図していない状態でもう一度 Esc すると、今度は選択ツールに戻る。
	m_toolManager->cancelCurrent();
	QVERIFY(dynamic_cast<SelectTool *>(m_toolManager->activeTool()) != nullptr);
}

void TestToolsIntegration::wireToolIgnoresClickOnSameVertexAgain() {
	m_toolManager->activateWireTool(WireKind::Bare);

	// 直前に置いた頂点と同じ格子点をもう一度クリックしても、長さ0の区間を
	// 作ってはいけない (以前は重複頂点がそのまま Wire::points に入っていた)。
	QTest::mouseClick(m_frontView->viewport(), Qt::LeftButton, Qt::NoModifier, viewPosFor(QPoint(20, 20)));
	QTest::mouseClick(m_frontView->viewport(), Qt::LeftButton, Qt::NoModifier, viewPosFor(QPoint(20, 20)));
	QTest::mouseClick(m_frontView->viewport(), Qt::LeftButton, Qt::NoModifier, viewPosFor(QPoint(60, 20)));
	QTest::mouseClick(m_frontView->viewport(), Qt::RightButton, Qt::NoModifier, viewPosFor(QPoint(60, 20)));

	QCOMPARE(m_doc->wires.size(), 1);
	QCOMPARE(m_doc->wires[0]->points.size(), 2);
	QCOMPARE(m_doc->wires[0]->points[0], QPoint(20, 20));
	QCOMPARE(m_doc->wires[0]->points[1], QPoint(60, 20));
}

void TestToolsIntegration::wireToolCountsSecondFastClickAsVertex() {
	m_toolManager->activateWireTool(WireKind::Bare);
	auto *vp = m_frontView->viewport();

	// 素早い2回目のクリック (ダブルクリック間隔・距離の内) は、押下ではなく
	// MouseButtonDblClick として届く。それも頂点として数えなければ、隣の格子点を
	// 続けて素早くクリックしたときに頂点が抜けてしまう。
	QTest::mouseClick(vp, Qt::LeftButton, Qt::NoModifier, viewPosFor(QPoint(20, 20)));
	QTest::mouseDClick(vp, Qt::LeftButton, Qt::NoModifier, viewPosFor(QPoint(60, 20)));
	QTest::mouseRelease(vp, Qt::LeftButton, Qt::NoModifier, viewPosFor(QPoint(60, 20)));
	// 同じ頂点への素早い再クリックは、これまでどおり長さ0の区間を作らない。
	QTest::mouseDClick(vp, Qt::LeftButton, Qt::NoModifier, viewPosFor(QPoint(60, 20)));
	QTest::mouseRelease(vp, Qt::LeftButton, Qt::NoModifier, viewPosFor(QPoint(60, 20)));
	QTest::mouseClick(vp, Qt::RightButton, Qt::NoModifier, viewPosFor(QPoint(60, 20)));

	QCOMPARE(m_doc->wires.size(), 1);
	QCOMPARE(m_doc->wires[0]->points.size(), 2);
	QCOMPARE(m_doc->wires[0]->points[0], QPoint(20, 20));
	QCOMPARE(m_doc->wires[0]->points[1], QPoint(60, 20));
}

void TestToolsIntegration::wireToolOverlayBoundsCoverConfirmedPointsAndCursor() {
	m_toolManager->activateWireTool(WireKind::Bare);

	QTest::mouseClick(m_frontView->viewport(), Qt::LeftButton, Qt::NoModifier, viewPosFor(QPoint(20, 20)));
	QTest::mouseMove(m_frontView->viewport(), viewPosFor(QPoint(20, 220)));

	// OverlayItem::boundingRect() は確定済みの頂点とカーソル位置の両方を覆っている
	// 必要がある。以前はカーソル周辺の固定 12x12 の箱しか返さず、離れた場所にある
	// 頂点やカーソルの軌跡が再描画されず画面に残ったままになるバグがあった。
	const QRectF bounds = m_frontScene->overlay()->boundingRect();
	QVERIFY2(bounds.height() > 100, qPrintable(QString("height=%1").arg(bounds.height())));
	QVERIFY(bounds.top() < 30);
	QVERIFY(bounds.bottom() > 210);

	m_toolManager->cancelCurrent();
}

void TestToolsIntegration::wireToolHidesRubberSegmentWhenMouseLeavesView() {
	m_toolManager->activateWireTool(WireKind::Bare);

	QTest::mouseClick(m_frontView->viewport(), Qt::LeftButton, Qt::NoModifier, viewPosFor(QPoint(20, 20)));
	QTest::mouseMove(m_frontView->viewport(), viewPosFor(QPoint(20, 220)));
	QVERIFY(m_frontScene->overlay()->boundingRect().bottom() > 210);

	// カーソルがビューの外に出たら、確定済みの頂点までのポリラインは残したまま、
	// そこから伸びる予告線 (ゴム線) だけを隠す。境界がカーソルの軌跡までは
	// 含まなくなり、最後の頂点付近まで縮むはず。
	m_frontScene->notifyViewLeave();
	QVERIFY(m_frontScene->overlay()->boundingRect().bottom() < 30);

	m_toolManager->cancelCurrent();
}

void TestToolsIntegration::selectToolStillHitsPartAndWireUnderDraftStroke() {
	auto placement = std::make_shared<Placement>();
	placement->uuid = "p1";
	placement->libraryId = m_libId;
	placement->partId = m_partId;
	placement->pos = QPoint(50, 50);
	m_doc->placements.append(placement);
	auto wire = std::make_shared<Wire>();
	wire->uuid = "w1";
	wire->layer = WireLayer::FrontBare;
	wire->points = {QPoint(20, 100), QPoint(100, 100)};
	m_doc->wires.append(wire);
	m_frontScene->syncPlacements();
	m_frontScene->syncWires();

	// 下書きで、部品と配線の両方を囲む矩形ができるストロークを描く。
	auto *vp = m_frontView->viewport();
	m_toolManager->activateDraftTool();
	QTest::mousePress(vp, Qt::LeftButton, Qt::NoModifier, viewPosFor(QPoint(30, 30)));
	dragMoveTo(viewPosFor(QPoint(80, 80)));
	dragMoveTo(viewPosFor(QPoint(130, 130)));
	QTest::mouseRelease(vp, Qt::LeftButton, Qt::NoModifier, viewPosFor(QPoint(130, 130)));
	QVERIFY(m_frontScene->overlay()->hasDraft());
	const QRectF bounds = m_frontScene->overlay()->boundingRect();
	QVERIFY(bounds.contains(QPointF(55, 55)) && bounds.contains(QPointF(60, 100)));

	// 選択ツールに戻すと、その矩形の中でも、部品と配線をこれまでどおりクリックで選択でき、
	// ヒットテスト (itemAt) にも最前面のオーバーレイではなく部品・配線が返る。
	m_toolManager->activateSelectTool();
	QVERIFY(dynamic_cast<PlacementItem *>(m_frontScene->itemAt(QPointF(55, 55), QTransform())) != nullptr);
	QVERIFY(dynamic_cast<WireItem *>(m_frontScene->itemAt(QPointF(60, 100), QTransform())) != nullptr);
	QTest::mouseClick(vp, Qt::LeftButton, Qt::NoModifier, viewPosFor(QPoint(60, 100)));
	QVERIFY(m_frontScene->wireItemFor("w1")->isSelected());
	QTest::mouseClick(vp, Qt::LeftButton, Qt::NoModifier, viewPosFor(QPoint(55, 55)));
	QVERIFY(m_frontScene->placementItemFor("p1")->isSelected());

	// ドラッグで部品を移動することもできる。
	QTest::mousePress(vp, Qt::LeftButton, Qt::NoModifier, viewPosFor(QPoint(55, 55)));
	dragMoveTo(viewPosFor(QPoint(85, 85)));
	QTest::mouseRelease(vp, Qt::LeftButton, Qt::NoModifier, viewPosFor(QPoint(85, 85)));
	QCOMPARE(m_doc->placements[0]->pos, QPoint(80, 80));
}

void TestToolsIntegration::draftStrokeIsCommittedWhenReleasedWithDifferentModifiers() {
	m_toolManager->activateDraftTool();
	auto *vp = m_frontView->viewport();

	// ストロークの途中で Shift を押し、押したままボタンを離す。離した時点の修飾キーが
	// 押下時と違っていても、ボタンを離せばストロークは確定する。
	QTest::mousePress(vp, Qt::LeftButton, Qt::NoModifier, viewPosFor(QPoint(30, 30)));
	dragMoveTo(viewPosFor(QPoint(60, 40)));
	dragMoveTo(viewPosFor(QPoint(90, 50)));
	QTest::mouseRelease(vp, Qt::LeftButton, Qt::ShiftModifier, viewPosFor(QPoint(90, 50)));
	QVERIFY(m_frontScene->overlay()->hasDraft());

	// 確定後は、ボタンを押さないカーソル移動でストロークが伸びたり動いたりしない。
	const QRectF committedBounds = m_frontScene->overlay()->boundingRect();
	dragMoveTo(viewPosFor(QPoint(250, 250)), Qt::NoButton);
	QCOMPARE(m_frontScene->overlay()->boundingRect(), committedBounds);
}

void TestToolsIntegration::draftStrokeIgnoresMovesWithoutTheButtonHeld() {
	m_toolManager->activateDraftTool();
	auto *vp = m_frontView->viewport();

	QTest::mousePress(vp, Qt::LeftButton, Qt::NoModifier, viewPosFor(QPoint(30, 30)));
	dragMoveTo(viewPosFor(QPoint(60, 40)));
	const QRectF boundsWhileDragging = m_frontScene->overlay()->boundingRect();

	// ボタンを離したのに release が届かなかった場合 (別ウィンドウの上で離した等)、
	// ボタンを押さないカーソル移動でストロークが伸び続けてはいけない。
	dragMoveTo(viewPosFor(QPoint(200, 200)), Qt::NoButton);
	QCOMPARE(m_frontScene->overlay()->boundingRect(), boundsWhileDragging);

	// 通常の release で確定でき、その後に新しいストロークも描ける。
	QTest::mouseRelease(vp, Qt::LeftButton, Qt::NoModifier, viewPosFor(QPoint(60, 40)));
	QVERIFY(m_frontScene->overlay()->hasDraft());
}

void TestToolsIntegration::spaceHeldLeftDragPansInsteadOfDrawingWire() {
	m_toolManager->activateWireTool(WireKind::Bare);
	auto *vp = m_frontView->viewport();
	m_frontView->centerOn(150, 150);
	const QPoint scrollBefore = scrollPosition();

	// Space 押下中の左ドラッグはビューのパン。配線ツールには押下が届かず、頂点も
	// プレビューも作られない (以前は押下をツールが消費してパンが始まらなかった)。
	QTest::keyPress(vp, Qt::Key_Space);
	QTest::mousePress(vp, Qt::LeftButton, Qt::NoModifier, QPoint(300, 300));
	QTest::mouseMove(vp, QPoint(280, 290));
	QTest::mouseMove(vp, QPoint(260, 280));
	QTest::mouseRelease(vp, Qt::LeftButton, Qt::NoModifier, QPoint(260, 280));
	QTest::keyRelease(vp, Qt::Key_Space);

	const QPoint scrollAfter = scrollPosition();
	QVERIFY(scrollAfter != scrollBefore);
	QVERIFY(m_frontScene->overlay()->boundingRect().isEmpty());

	// Space を離したあとの左クリックは、また頂点になる。
	QTest::mouseClick(vp, Qt::LeftButton, Qt::NoModifier, viewPosFor(QPoint(20, 20)));
	QVERIFY(!m_frontScene->overlay()->boundingRect().isEmpty());
	QTest::mouseClick(vp, Qt::LeftButton, Qt::NoModifier, viewPosFor(QPoint(60, 20)));
	QTest::mouseClick(vp, Qt::RightButton, Qt::NoModifier, viewPosFor(QPoint(60, 20)));
	QCOMPARE(m_doc->wires.size(), 1);
	QCOMPARE(m_doc->wires[0]->points.size(), 2);
}

void TestToolsIntegration::spaceHeldDoubleClickAlsoPansInsteadOfDrawingWire() {
	m_toolManager->activateWireTool(WireKind::Bare);
	auto *vp = m_frontView->viewport();
	m_frontView->centerOn(150, 150);
	const QPoint scrollBefore = scrollPosition();

	// 素早い2回目の押下はダブルクリックとして届く。それもパンとして扱う。
	QTest::keyPress(vp, Qt::Key_Space);
	QTest::mouseDClick(vp, Qt::LeftButton, Qt::NoModifier, QPoint(300, 300));
	QTest::mouseMove(vp, QPoint(280, 290));
	QTest::mouseMove(vp, QPoint(260, 280));
	QTest::mouseRelease(vp, Qt::LeftButton, Qt::NoModifier, QPoint(260, 280));
	QTest::keyRelease(vp, Qt::Key_Space);

	const QPoint scrollAfter = scrollPosition();
	QVERIFY(scrollAfter != scrollBefore);
	QVERIFY(m_frontScene->overlay()->boundingRect().isEmpty());
}

void TestToolsIntegration::spaceHeldIsClearedWhenViewLosesFocus() {
	m_toolManager->activateWireTool(WireKind::Bare);
	auto *vp = m_frontView->viewport();

	// Space を押したままフォーカスが他へ移ると keyRelease は届かない。Space 押下中の扱いが
	// 残ったままだと、以後の左クリックがすべてパンになって配線を引けなくなる。
	QTest::keyPress(vp, Qt::Key_Space);
	QFocusEvent focusOut(QEvent::FocusOut);
	QApplication::sendEvent(m_frontView.get(), &focusOut);

	QTest::mouseClick(vp, Qt::LeftButton, Qt::NoModifier, viewPosFor(QPoint(20, 20)));
	QVERIFY(!m_frontScene->overlay()->boundingRect().isEmpty());
	m_toolManager->cancelCurrent();
}

void TestToolsIntegration::middleButtonPanEndsEvenIfModifiersChangeBeforeRelease() {
	auto *vp = m_frontView->viewport();
	m_frontView->centerOn(150, 150);

	// 中ボタンでパンし、途中で押した Ctrl を保ったままボタンを離す。離した時点の修飾キーが
	// 押下時と違っても、パンは終わらなければならない。
	QTest::mousePress(vp, Qt::MiddleButton, Qt::NoModifier, QPoint(300, 300));
	QTest::mouseMove(vp, QPoint(280, 290));
	QTest::mouseRelease(vp, Qt::MiddleButton, Qt::ControlModifier, QPoint(280, 290));
	const QPoint scrollAtRelease = scrollPosition();

	// ボタンを押さない移動でスクロールし続けてはいけない。
	QTest::mouseMove(vp, QPoint(240, 250));
	const QPoint scrollAfterMove = scrollPosition();
	QCOMPARE(scrollAfterMove, scrollAtRelease);
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

void TestToolsIntegration::escapeReturnsPlacePartToolToSelectTool() {
	m_toolManager->activatePlacePartTool(m_libId, m_partId);
	QVERIFY(dynamic_cast<PlacePartTool *>(m_toolManager->activeTool()) != nullptr);

	m_toolManager->cancelCurrent();
	QVERIFY(dynamic_cast<SelectTool *>(m_toolManager->activeTool()) != nullptr);
}

void TestToolsIntegration::escapeAfterWireCommitChecksSelectToolButton() {
	MainWindow window;
	window.resize(1200, 800);
	window.show();
	QVERIFY(QTest::qWaitForWindowExposed(&window));
	window.activateWindow();
	QVERIFY(QTest::qWaitForWindowActive(&window));
	// 起動直後に遅延実行される全体表示 (fitViewsDeferred) を済ませてからクリック位置を求める。
	QTest::qWait(10);

	auto *toolManager = window.findChild<ToolManager *>();
	QVERIFY(toolManager != nullptr);
	QAction *selectButton = window.actionRegistry().action("toolbar.tools.select");
	QAction *wireButton = window.actionRegistry().action("toolbar.tools.wireBare");
	QVERIFY(selectButton != nullptr && wireButton != nullptr);

	wireButton->trigger();
	QVERIFY(wireButton->isChecked());

	// 配線を1本引いて確定する。確定後も配線ツールのまま。
	BoardScene *scene = toolManager->context().frontScene;
	auto *view = qobject_cast<BoardView *>(scene->views().value(0));
	QVERIFY(view != nullptr);
	const QPoint from = view->mapFromScene(scene->fromModel(view->viewCenterModel()));
	const QPoint to = from + QPoint(40, 0);
	QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, from);
	QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, to);
	QTest::mouseClick(view->viewport(), Qt::RightButton, Qt::NoModifier, to);
	QCOMPARE(toolManager->context().document->wires.size(), 1);
	QVERIFY(dynamic_cast<WireTool *>(toolManager->activeTool()) != nullptr);
	QVERIFY(wireButton->isChecked());

	// 作図中のものが無い状態で Esc を押すと選択ツールに戻る。ツールバーの押下表示も
	// 「配線」のままではなく「選択」に変わらなければならない。
	QTest::keyClick(&window, Qt::Key_Escape);
	QVERIFY(dynamic_cast<SelectTool *>(toolManager->activeTool()) != nullptr);
	QVERIFY(selectButton->isChecked());
	QVERIFY(!wireButton->isChecked());
}

void TestToolsIntegration::toolbarButtonsFollowTheActiveTool() {
	MainWindow window;
	auto *toolManager = window.findChild<ToolManager *>();
	QVERIFY(toolManager != nullptr);

	const QStringList buttonIds = {"toolbar.tools.select", "toolbar.tools.wireBare", "toolbar.tools.wireInsulated",
								   "toolbar.tools.wireOutline", "toolbar.tools.draft"};
	auto checkedButtons = [&] {
		QStringList checked;
		for (const QString &id : buttonIds) {
			if (window.actionRegistry().action(id)->isChecked()) {
				checked << id;
			}
		}
		return checked;
	};

	// ボタン以外 (ToolManager を直接操作 = Esc・右クリック・部品パレット等と同じ経路) で
	// ツールが変わっても、対応するボタンだけが押下状態になる。
	QCOMPARE(checkedButtons(), QStringList{"toolbar.tools.select"});
	toolManager->activateWireTool(WireKind::Insulated);
	QCOMPARE(checkedButtons(), QStringList{"toolbar.tools.wireInsulated"});
	toolManager->activateWireTool(WireKind::Outline);
	QCOMPARE(checkedButtons(), QStringList{"toolbar.tools.wireOutline"});
	toolManager->activateWireTool(WireKind::Bare);
	QCOMPARE(checkedButtons(), QStringList{"toolbar.tools.wireBare"});
	toolManager->activateDraftTool();
	QCOMPARE(checkedButtons(), QStringList{"toolbar.tools.draft"});
	// 部品配置ツールに対応するボタンは無いので、どれも押下状態にならない。
	toolManager->activatePlacePartTool("lib", "part");
	QCOMPARE(checkedButtons(), QStringList{});
	toolManager->activateSelectTool();
	QCOMPARE(checkedButtons(), QStringList{"toolbar.tools.select"});

	// ボタンを押したときの動作は従来どおり。押したボタンのツールに切り替わり、そのボタンが
	// 押下状態になる。すでに押下状態のボタンをもう一度押しても、そのままそのツールが有効。
	window.actionRegistry().action("toolbar.tools.draft")->trigger();
	QVERIFY(dynamic_cast<DraftTool *>(toolManager->activeTool()) != nullptr);
	QCOMPARE(checkedButtons(), QStringList{"toolbar.tools.draft"});
	window.actionRegistry().action("toolbar.tools.wireOutline")->trigger();
	auto *wireTool = dynamic_cast<WireTool *>(toolManager->activeTool());
	QVERIFY(wireTool != nullptr);
	QCOMPARE(static_cast<int>(wireTool->kind()), static_cast<int>(WireKind::Outline));
	QCOMPARE(checkedButtons(), QStringList{"toolbar.tools.wireOutline"});
	window.actionRegistry().action("toolbar.tools.wireOutline")->trigger();
	QVERIFY(dynamic_cast<WireTool *>(toolManager->activeTool()) != nullptr);
	QCOMPARE(checkedButtons(), QStringList{"toolbar.tools.wireOutline"});
}

void TestToolsIntegration::backSceneCoordinatesAreNotMirrored() {
	// 裏面ビューで「見た目上モデル座標 (50,50) にあたる位置」をクリックしたとき、
	// 実際にモデル座標 (50,50) に配置されること。裏面シーンでは m_root に水平反転
	// (QTransform(-1,0,0,1,size.width(),0)) がかかっているため、そのシーン座標は
	// (size.width()-50, 50) になる。ツールが toModel() を経由せず生の scenePos() を
	// そのままモデル座標として使ってしまうと、ここで反転した位置に置かれてしまう
	// (このバグの回帰テスト)。
	auto backView = std::make_unique<BoardView>();
	backView->setScene(m_backScene.get());
	backView->resize(600, 600);
	backView->show();
	QVERIFY(QTest::qWaitForWindowExposed(backView.get()));
	backView->resetZoom();

	m_toolManager->activatePlacePartTool(m_libId, m_partId);
	const QPointF targetModel(50, 50);
	const QPointF targetScene = m_backScene->fromModel(targetModel);
	QVERIFY(!qFuzzyCompare(targetScene.x(), targetModel.x()));  // 前提: 実際に反転している
	const QPoint clickAt = backView->mapFromScene(targetScene);
	QTest::mouseClick(backView->viewport(), Qt::LeftButton, Qt::NoModifier, clickAt);

	QCOMPARE(m_doc->placements.size(), 1);
	QCOMPARE(m_doc->placements[0]->pos, QPoint(50, 50));
	QCOMPARE(m_doc->placements[0]->side, Side::Back);
}

QTEST_MAIN(TestToolsIntegration)
#include "test_tools_integration.moc"
