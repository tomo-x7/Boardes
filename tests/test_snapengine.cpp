#include <QTest>

#include "model/board.h"
#include "ui/tools/snapengine.h"

class TestSnapEngine : public QObject {
	Q_OBJECT

private slots:
	void snapsToNearestOfEightDirections();
	void roundsDistanceToGranularityStep();
	void handlesNegativeDirections();
	void halfGranularityUsesFiveUnitSteps();
	void freeGranularityUsesOneUnitSteps();
	void snapsToGridUsingBoardOrigin();
	void snapToGridFallsBackToSceneOriginWithoutBoard();
	void snapForPlacementSubtractsAnchor();
};

void TestSnapEngine::snapsToNearestOfEightDirections() {
	SnapEngine snap;  // 既定 Full (10単位)

	// ほぼ水平 -> 真水平にスナップ
	QCOMPARE(snap.snapForWireVertex(QPoint(0, 0), QPointF(100, 5)), QPoint(100, 0));
	// ほぼ垂直 -> 真垂直にスナップ
	QCOMPARE(snap.snapForWireVertex(QPoint(0, 0), QPointF(5, 100)), QPoint(0, 100));
	// ほぼ45度 -> 真の45度にスナップ (投影距離 ≈72.1 -> 7段 -> 70,70)
	QCOMPARE(snap.snapForWireVertex(QPoint(0, 0), QPointF(50, 52)), QPoint(70, 70));
}

void TestSnapEngine::roundsDistanceToGranularityStep() {
	SnapEngine snap;
	// 水平方向、距離23 -> 10単位に丸めて20でなく (23/10=2.3->round2? いや2.3は2.3で
	// round(2.3)=2なので20)。実際に検証する。
	QCOMPARE(snap.snapForWireVertex(QPoint(0, 0), QPointF(23, 0)), QPoint(20, 0));
	// 距離26 -> round(2.6)=3 -> 30
	QCOMPARE(snap.snapForWireVertex(QPoint(0, 0), QPointF(26, 0)), QPoint(30, 0));
}

void TestSnapEngine::handlesNegativeDirections() {
	SnapEngine snap;
	QCOMPARE(snap.snapForWireVertex(QPoint(0, 0), QPointF(-30, -3)), QPoint(-30, 0));
	// 左下45度、距離=30*sqrt(2)≈42.4 -> 10単位で4段 -> (50,50)から(-40,-40)
	QCOMPARE(snap.snapForWireVertex(QPoint(50, 50), QPointF(20, 20)), QPoint(10, 10));
}

void TestSnapEngine::halfGranularityUsesFiveUnitSteps() {
	SnapEngine snap;
	snap.setGranularity(units::Granularity::Half);
	QCOMPARE(snap.snapForWireVertex(QPoint(0, 0), QPointF(23, 2)), QPoint(25, 0));
}

void TestSnapEngine::freeGranularityUsesOneUnitSteps() {
	SnapEngine snap;
	snap.setGranularity(units::Granularity::Free);
	QCOMPARE(snap.snapForWireVertex(QPoint(0, 0), QPointF(23, 1)), QPoint(23, 0));
}

void TestSnapEngine::snapsToGridUsingBoardOrigin() {
	BoardSpec board;
	board.origin = QPoint(5, 5);
	board.pitch = 10;
	SnapEngine snap;
	snap.setBoard(&board);

	// origin=(5,5) の格子点は 5,15,25,... であり、シーン原点 (0,0) 基準の 10 単位刻みとは
	// 常に半グリッドずれる。ここが Phase 11 で直したバグの核心。
	QCOMPARE(snap.snapToGrid(QPointF(6, 6), 10), QPoint(5, 5));
	QCOMPARE(snap.snapToGrid(QPointF(14, 14), 10), QPoint(15, 15));
	QCOMPARE(snap.snapToGrid(QPointF(24, 26), 10), QPoint(25, 25));
}

void TestSnapEngine::snapToGridFallsBackToSceneOriginWithoutBoard() {
	SnapEngine snap;  // board 未設定 (nullptr)
	QCOMPARE(snap.snapToGrid(QPointF(4, 6), 10), QPoint(0, 10));
	QCOMPARE(snap.snapToGrid(QPointF(23, 0), 10), QPoint(20, 0));
}

void TestSnapEngine::snapForPlacementSubtractsAnchor() {
	BoardSpec board;
	board.origin = QPoint(5, 5);
	board.pitch = 10;
	SnapEngine snap;
	snap.setBoard(&board);

	// アンカー (2,3) が穴 (15,15) に一致するような部品原点は (13,12)。
	const QPoint anchor(2, 3);
	QCOMPARE(snap.snapForPlacement(QPointF(14, 14), anchor), QPoint(13, 12));
}

QTEST_MAIN(TestSnapEngine)
#include "test_snapengine.moc"
