#include <QTest>

#include "commands/boardcommands.h"
#include "commands/placementcommands.h"
#include "commands/wirecommands.h"
#include "model/document.h"
#include "render/boardscene.h"
#include "render/items/placementitem.h"
#include "render/items/wireitem.h"
#include "ui/tools/toolcontext.h"

namespace {

std::shared_ptr<Placement> makePlacement(const QString &uuid, QPoint pos) {
	auto p = std::make_shared<Placement>();
	p->uuid = uuid;
	p->libraryId = "lib";
	p->partId = "part";
	p->pos = pos;
	p->refDes = "R1";
	p->value = "10k";
	return p;
}

std::shared_ptr<Wire> makeWire(const QString &uuid, WireLayer layer) {
	auto w = std::make_shared<Wire>();
	w->uuid = uuid;
	w->layer = layer;
	w->points = {QPoint(0, 0), QPoint(10, 0)};
	return w;
}

}  // namespace

class TestCommands : public QObject {
	Q_OBJECT

private slots:
	void init();
	void cleanup();

	void addPlacementUndoRedo();
	void removePlacementPreservesDataAndPosition();
	void movePlacementUndoRedo();
	void rotatePlacementUndoRedo();
	void flipPlacementSideUndoRedo();
	void setLabelUndoRedo();
	void setPlacementVisibleUndoRedo();
	void setWireVisibleUndoRedo();
	void addWireUndoRedo();
	void removeWirePreservesData();
	void changeWireLayerUndoRedo();
	void macroUndoesAllAtOnce();
	void longSequenceRoundTrips();
	void setBoardUndoRedoResyncsScenes();

private:
	std::unique_ptr<Document> m_doc;
	std::unique_ptr<BoardScene> m_front;
	std::unique_ptr<BoardScene> m_back;
	std::unique_ptr<ToolContext> m_ctx;
};

void TestCommands::init() {
	m_doc = std::make_unique<Document>();
	m_doc->board.size = QSize(200, 200);
	m_front = std::make_unique<BoardScene>(Side::Front);
	m_back = std::make_unique<BoardScene>(Side::Back);
	m_front->setDocument(m_doc.get(), nullptr);
	m_back->setDocument(m_doc.get(), nullptr);

	m_ctx = std::make_unique<ToolContext>();
	m_ctx->document = m_doc.get();
	m_ctx->frontScene = m_front.get();
	m_ctx->backScene = m_back.get();
}

void TestCommands::cleanup() {
	m_ctx.reset();
	m_back.reset();
	m_front.reset();
	m_doc.reset();
}

void TestCommands::addPlacementUndoRedo() {
	auto *stack = m_doc->undoStack();
	stack->push(new AddPlacementCommand(m_ctx.get(), makePlacement("p1", QPoint(10, 10))));
	QCOMPARE(m_doc->placements.size(), 1);
	QVERIFY(m_front->placementItemFor("p1") != nullptr);

	stack->undo();
	QCOMPARE(m_doc->placements.size(), 0);
	QVERIFY(m_front->placementItemFor("p1") == nullptr);

	stack->redo();
	QCOMPARE(m_doc->placements.size(), 1);
	QCOMPARE(m_doc->placements[0]->pos, QPoint(10, 10));
}

void TestCommands::removePlacementPreservesDataAndPosition() {
	m_doc->placements.append(makePlacement("p1", QPoint(0, 0)));
	m_doc->placements.append(makePlacement("p2", QPoint(10, 10)));
	m_doc->placements.append(makePlacement("p3", QPoint(20, 20)));
	m_front->syncPlacements();

	auto *stack = m_doc->undoStack();
	stack->push(new RemovePlacementCommand(m_ctx.get(), "p2"));
	QCOMPARE(m_doc->placements.size(), 2);
	QCOMPARE(m_doc->placements[0]->uuid, QStringLiteral("p1"));
	QCOMPARE(m_doc->placements[1]->uuid, QStringLiteral("p3"));

	stack->undo();
	QCOMPARE(m_doc->placements.size(), 3);
	// 元の位置 (index 1) に正しく戻ること。
	QCOMPARE(m_doc->placements[1]->uuid, QStringLiteral("p2"));
	QCOMPARE(m_doc->placements[1]->pos, QPoint(10, 10));
}

void TestCommands::movePlacementUndoRedo() {
	m_doc->placements.append(makePlacement("p1", QPoint(5, 5)));
	m_front->syncPlacements();

	auto *stack = m_doc->undoStack();
	stack->push(new MovePlacementCommand(m_ctx.get(), "p1", QPoint(5, 5), QPoint(50, 60)));
	QCOMPARE(m_doc->placements[0]->pos, QPoint(50, 60));
	QCOMPARE(m_front->placementItemFor("p1")->pos(), QPointF(50, 60));

	stack->undo();
	QCOMPARE(m_doc->placements[0]->pos, QPoint(5, 5));
	QCOMPARE(m_front->placementItemFor("p1")->pos(), QPointF(5, 5));
}

void TestCommands::rotatePlacementUndoRedo() {
	m_doc->placements.append(makePlacement("p1", QPoint(0, 0)));
	m_front->syncPlacements();

	auto *stack = m_doc->undoStack();
	stack->push(new RotatePlacementCommand(m_ctx.get(), "p1", Rotation::R0, Rotation::R90));
	QCOMPARE(static_cast<int>(m_doc->placements[0]->rot), static_cast<int>(Rotation::R90));
	stack->undo();
	QCOMPARE(static_cast<int>(m_doc->placements[0]->rot), static_cast<int>(Rotation::R0));
}

void TestCommands::flipPlacementSideUndoRedo() {
	m_doc->placements.append(makePlacement("p1", QPoint(0, 0)));
	m_doc->placements[0]->side = Side::Front;
	m_front->syncPlacements();

	auto *stack = m_doc->undoStack();
	stack->push(new FlipPlacementSideCommand(m_ctx.get(), "p1", Side::Front, Side::Back));
	QCOMPARE(static_cast<int>(m_doc->placements[0]->side), static_cast<int>(Side::Back));
	stack->undo();
	QCOMPARE(static_cast<int>(m_doc->placements[0]->side), static_cast<int>(Side::Front));
}

void TestCommands::setLabelUndoRedo() {
	m_doc->placements.append(makePlacement("p1", QPoint(0, 0)));
	m_front->syncPlacements();

	auto *stack = m_doc->undoStack();
	stack->push(new SetPlacementLabelCommand(m_ctx.get(), "p1", "R1", "10k", "R99", "4.7k"));
	QCOMPARE(m_doc->placements[0]->refDes, QStringLiteral("R99"));
	QCOMPARE(m_doc->placements[0]->value, QStringLiteral("4.7k"));
	stack->undo();
	QCOMPARE(m_doc->placements[0]->refDes, QStringLiteral("R1"));
	QCOMPARE(m_doc->placements[0]->value, QStringLiteral("10k"));
}

void TestCommands::setPlacementVisibleUndoRedo() {
	m_doc->placements.append(makePlacement("p1", QPoint(0, 0)));
	m_front->syncPlacements();
	QVERIFY(m_doc->placements[0]->visible);
	QVERIFY(m_front->placementItemFor("p1")->isVisible());

	auto *stack = m_doc->undoStack();
	stack->push(new SetPlacementVisibleCommand(m_ctx.get(), "p1", true, false));
	QVERIFY(!m_doc->placements[0]->visible);
	QVERIFY(!m_front->placementItemFor("p1")->isVisible());

	stack->undo();
	QVERIFY(m_doc->placements[0]->visible);
	QVERIFY(m_front->placementItemFor("p1")->isVisible());

	stack->redo();
	QVERIFY(!m_doc->placements[0]->visible);
}

void TestCommands::setWireVisibleUndoRedo() {
	m_doc->wires.append(makeWire("w1", WireLayer::FrontBare));
	m_front->syncWires();
	QVERIFY(m_doc->wires[0]->visible);
	QVERIFY(m_front->wireItemFor("w1")->isVisible());

	auto *stack = m_doc->undoStack();
	stack->push(new SetWireVisibleCommand(m_ctx.get(), "w1", true, false));
	QVERIFY(!m_doc->wires[0]->visible);
	QVERIFY(!m_front->wireItemFor("w1")->isVisible());

	stack->undo();
	QVERIFY(m_doc->wires[0]->visible);
	QVERIFY(m_front->wireItemFor("w1")->isVisible());
}

void TestCommands::addWireUndoRedo() {
	auto *stack = m_doc->undoStack();
	stack->push(new AddWireCommand(m_ctx.get(), makeWire("w1", WireLayer::FrontBare)));
	QCOMPARE(m_doc->wires.size(), 1);
	QVERIFY(m_front->wireItemFor("w1") != nullptr);
	stack->undo();
	QCOMPARE(m_doc->wires.size(), 0);
	QVERIFY(m_front->wireItemFor("w1") == nullptr);
}

void TestCommands::removeWirePreservesData() {
	m_doc->wires.append(makeWire("w1", WireLayer::FrontBare));
	m_front->syncWires();

	auto *stack = m_doc->undoStack();
	stack->push(new RemoveWireCommand(m_ctx.get(), "w1"));
	QCOMPARE(m_doc->wires.size(), 0);
	stack->undo();
	QCOMPARE(m_doc->wires.size(), 1);
	QCOMPARE(m_doc->wires[0]->points.size(), 2);
}

void TestCommands::changeWireLayerUndoRedo() {
	m_doc->wires.append(makeWire("w1", WireLayer::FrontBare));
	m_front->syncWires();

	auto *stack = m_doc->undoStack();
	stack->push(new ChangeWireLayerCommand(m_ctx.get(), "w1", WireLayer::FrontBare, WireLayer::BackInsulated));
	QCOMPARE(static_cast<int>(m_doc->wires[0]->layer), static_cast<int>(WireLayer::BackInsulated));
	// 層が変わったので front シーンからは消え、back シーンに現れる。
	QVERIFY(m_front->wireItemFor("w1") == nullptr);
	QVERIFY(m_back->wireItemFor("w1") != nullptr);

	stack->undo();
	QCOMPARE(static_cast<int>(m_doc->wires[0]->layer), static_cast<int>(WireLayer::FrontBare));
	QVERIFY(m_front->wireItemFor("w1") != nullptr);
	QVERIFY(m_back->wireItemFor("w1") == nullptr);
}

void TestCommands::macroUndoesAllAtOnce() {
	auto *stack = m_doc->undoStack();
	stack->beginMacro("複数追加");
	stack->push(new AddPlacementCommand(m_ctx.get(), makePlacement("p1", QPoint(0, 0))));
	stack->push(new AddPlacementCommand(m_ctx.get(), makePlacement("p2", QPoint(10, 10))));
	stack->push(new AddPlacementCommand(m_ctx.get(), makePlacement("p3", QPoint(20, 20))));
	stack->endMacro();

	QCOMPARE(m_doc->placements.size(), 3);
	stack->undo();  // マクロ全体を1回で取り消せること
	QCOMPARE(m_doc->placements.size(), 0);
	stack->redo();
	QCOMPARE(m_doc->placements.size(), 3);
}

void TestCommands::longSequenceRoundTrips() {
	auto *stack = m_doc->undoStack();
	stack->push(new AddPlacementCommand(m_ctx.get(), makePlacement("p1", QPoint(0, 0))));
	stack->push(new MovePlacementCommand(m_ctx.get(), "p1", QPoint(0, 0), QPoint(30, 30)));
	stack->push(new RotatePlacementCommand(m_ctx.get(), "p1", Rotation::R0, Rotation::R180));
	stack->push(new AddWireCommand(m_ctx.get(), makeWire("w1", WireLayer::FrontBare)));
	stack->push(new FlipPlacementSideCommand(m_ctx.get(), "p1", Side::Front, Side::Back));

	QCOMPARE(m_doc->placements[0]->pos, QPoint(30, 30));
	QCOMPARE(static_cast<int>(m_doc->placements[0]->rot), static_cast<int>(Rotation::R180));
	QCOMPARE(static_cast<int>(m_doc->placements[0]->side), static_cast<int>(Side::Back));
	QCOMPARE(m_doc->wires.size(), 1);

	for (int i = 0; i < 5; ++i) stack->undo();
	QCOMPARE(m_doc->placements.size(), 0);
	QCOMPARE(m_doc->wires.size(), 0);

	for (int i = 0; i < 5; ++i) stack->redo();
	QCOMPARE(m_doc->placements[0]->pos, QPoint(30, 30));
	QCOMPARE(static_cast<int>(m_doc->placements[0]->rot), static_cast<int>(Rotation::R180));
	QCOMPARE(static_cast<int>(m_doc->placements[0]->side), static_cast<int>(Side::Back));
	QCOMPARE(m_doc->wires.size(), 1);
}

void TestCommands::setBoardUndoRedoResyncsScenes() {
	BoardSpec newBoard;
	newBoard.id = "NEWB";
	newBoard.size = QSize(80, 80);
	newBoard.cols = 6;
	newBoard.rows = 6;
	newBoard.pitch = 10;

	const BoardSpec oldBoard = m_doc->board;
	auto *stack = m_doc->undoStack();
	stack->push(new SetBoardCommand(m_ctx.get(), oldBoard, newBoard));

	QCOMPARE(m_doc->board.id, QStringLiteral("NEWB"));
	QCOMPARE(m_front->document()->board.id, QStringLiteral("NEWB"));  // シーン側も再同期される

	stack->undo();
	QCOMPARE(m_doc->board.id, oldBoard.id);
	stack->redo();
	QCOMPARE(m_doc->board.id, QStringLiteral("NEWB"));
}

QTEST_MAIN(TestCommands)
#include "test_commands.moc"
