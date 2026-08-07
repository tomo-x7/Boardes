#include <QImage>
#include <QTest>

#include "model/document.h"
#include "model/librarymanager.h"
#include "render/boardscene.h"
#include "render/items/labelitem.h"
#include "render/items/placementitem.h"
#include "render/items/wireitem.h"

// BoardScene の鏡像化まわりは目視確認が難しく間違えやすいので、変換行列そのものを
// 直接検証する。詳細は BoardScene/LabelItem のコメントを参照:
//   - 裏面シーンは rootItem() に水平反転 (-1,0,0,1,boardWidth,0) がかかる。
//   - 通常のアイテム (PlacementItem 等) はその反転をそのまま受け継ぐ (sceneTransform().m11() < 0)。
//   - LabelItem だけは自身にも逆向きの反転をかけて打ち消し、文字が読める向きになる
//     (sceneTransform().m11() > 0 に戻る)。
class TestBoardScene : public QObject {
	Q_OBJECT

private slots:
	void frontSceneRootHasIdentityTransform();
	void backSceneRootHasHorizontalMirror();
	void placementItemInheritsMirrorOnBackScene();
	void labelItemCancelsMirrorOnBackScene();
	void wiresAreRoutedToTheCorrectSideScene();
	void removingPlacementRemovesItsRenderItem();

private:
	std::unique_ptr<Document> makeTestDocument();
};

std::unique_ptr<Document> TestBoardScene::makeTestDocument() {
	auto doc = std::make_unique<Document>();
	doc->board.id = "TB";
	doc->board.size = QSize(100, 80);
	doc->board.cols = 8;
	doc->board.rows = 6;
	doc->board.pitch = 10;
	doc->board.origin = QPoint(10, 10);

	auto placement = std::make_shared<Placement>();
	placement->uuid = "p1";
	placement->libraryId = "lib";
	placement->partId = "part1";
	placement->pos = QPoint(20, 20);
	placement->side = Side::Front;
	placement->refDes = "R1";
	placement->value = "10k";
	doc->placements.append(placement);

	auto frontWire = std::make_shared<Wire>();
	frontWire->uuid = "w1";
	frontWire->layer = WireLayer::FrontBare;
	frontWire->points = {QPoint(0, 0), QPoint(10, 0)};
	doc->wires.append(frontWire);

	auto backWire = std::make_shared<Wire>();
	backWire->uuid = "w2";
	backWire->layer = WireLayer::BackBare;
	backWire->points = {QPoint(0, 0), QPoint(10, 0)};
	doc->wires.append(backWire);

	auto outlineWire = std::make_shared<Wire>();
	outlineWire->uuid = "w3";
	outlineWire->layer = WireLayer::Outline;
	outlineWire->points = {QPoint(0, 0), QPoint(0, 10)};
	doc->wires.append(outlineWire);

	return doc;
}

void TestBoardScene::frontSceneRootHasIdentityTransform() {
	auto doc = makeTestDocument();
	BoardScene scene(Side::Front);
	scene.setDocument(doc.get(), nullptr);
	QVERIFY(scene.rootItem()->transform().isIdentity());
}

void TestBoardScene::backSceneRootHasHorizontalMirror() {
	auto doc = makeTestDocument();
	BoardScene scene(Side::Back);
	scene.setDocument(doc.get(), nullptr);
	const QTransform t = scene.rootItem()->transform();
	QCOMPARE(t.m11(), -1.0);
	QCOMPARE(t.m22(), 1.0);
	QCOMPARE(t.dx(), 100.0);  // board.size.width()
}

void TestBoardScene::placementItemInheritsMirrorOnBackScene() {
	auto doc = makeTestDocument();
	BoardScene frontScene(Side::Front);
	frontScene.setDocument(doc.get(), nullptr);
	BoardScene backScene(Side::Back);
	backScene.setDocument(doc.get(), nullptr);

	auto *frontItem = frontScene.placementItemFor("p1");
	auto *backItem = backScene.placementItemFor("p1");
	QVERIFY(frontItem != nullptr);
	QVERIFY(backItem != nullptr);

	QCOMPARE(frontItem->sceneTransform().m11(), 1.0);
	QCOMPARE(backItem->sceneTransform().m11(), -1.0);  // 反転を継承している
}

void TestBoardScene::labelItemCancelsMirrorOnBackScene() {
	auto doc = makeTestDocument();
	BoardScene backScene(Side::Back);
	backScene.setDocument(doc.get(), nullptr);

	// LabelItem は uuid で公開していないので、rootItem の子から探す。
	LabelItem *label = nullptr;
	const auto children = backScene.rootItem()->childItems();
	for (auto *child : children) {
		if (auto *l = dynamic_cast<LabelItem *>(child)) {
			label = l;
			break;
		}
	}
	QVERIFY(label != nullptr);
	// 親 (root) は反転しているが、ラベル自身の合成変換では打ち消されて m11 > 0 に戻る。
	QVERIFY(label->sceneTransform().m11() > 0.0);
}

void TestBoardScene::wiresAreRoutedToTheCorrectSideScene() {
	auto doc = makeTestDocument();
	BoardScene frontScene(Side::Front);
	frontScene.setDocument(doc.get(), nullptr);
	BoardScene backScene(Side::Back);
	backScene.setDocument(doc.get(), nullptr);

	QVERIFY(frontScene.wireItemFor("w1") != nullptr);   // frontBare -> front のみ
	QVERIFY(frontScene.wireItemFor("w2") == nullptr);
	QVERIFY(backScene.wireItemFor("w1") == nullptr);
	QVERIFY(backScene.wireItemFor("w2") != nullptr);    // backBare -> back のみ

	QVERIFY(frontScene.wireItemFor("w3") != nullptr);   // outline -> 両面
	QVERIFY(backScene.wireItemFor("w3") != nullptr);
}

void TestBoardScene::removingPlacementRemovesItsRenderItem() {
	auto doc = makeTestDocument();
	BoardScene scene(Side::Front);
	scene.setDocument(doc.get(), nullptr);
	QVERIFY(scene.placementItemFor("p1") != nullptr);

	doc->placements.clear();
	scene.syncPlacements();
	QVERIFY(scene.placementItemFor("p1") == nullptr);
}

QTEST_MAIN(TestBoardScene)
#include "test_boardscene.moc"
