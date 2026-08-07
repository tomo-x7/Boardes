#include <QImage>
#include <QTemporaryDir>
#include <QTest>
#include <algorithm>

#include "model/document.h"
#include "model/drc.h"
#include "model/librarymanager.h"

namespace {

std::shared_ptr<Part> makePinPart(const QString &id, QVector<Pin> pins, PartKind kind = PartKind::Normal,
								   QSize size = QSize(10, 10)) {
	auto part = std::make_shared<Part>();
	part->id = id;
	part->name = id;
	part->kind = kind;
	part->outline = QRect(QPoint(0, 0), size);
	QImage img(size, QImage::Format_RGB888);
	img.fill(Qt::gray);
	part->artwork = Artwork::fromImageAsIs(img);
	part->pins = std::move(pins);
	return part;
}

std::shared_ptr<Placement> place(const QString &uuid, const QString &libId, const QString &partId, QPoint pos,
								 Side side = Side::Front) {
	auto p = std::make_shared<Placement>();
	p->uuid = uuid;
	p->libraryId = libId;
	p->partId = partId;
	p->pos = pos;
	p->side = side;
	p->refDes = uuid;
	return p;
}

std::shared_ptr<Wire> makeWire(const QString &uuid, WireLayer layer, QVector<QPoint> points) {
	auto w = std::make_shared<Wire>();
	w->uuid = uuid;
	w->layer = layer;
	w->points = std::move(points);
	return w;
}

bool hasRule(const QVector<DrcFinding> &findings, const QString &ruleId) {
	return std::any_of(findings.begin(), findings.end(), [&](const DrcFinding &f) { return f.ruleId == ruleId; });
}

int countRule(const QVector<DrcFinding> &findings, const QString &ruleId) {
	return static_cast<int>(std::count_if(findings.begin(), findings.end(),
										   [&](const DrcFinding &f) { return f.ruleId == ruleId; }));
}

}  // namespace

// DrcEngine の7ルールを、各ルールにつき陽性1件・陰性1件で検証する。
class TestDrc : public QObject {
	Q_OBJECT

private slots:
	void init();

	void unconnectedPinIsWarned();
	void fullyConnectedPinsAreNotWarned();

	void sharedHoleIsError();
	void distinctHolesAreNotFlagged();

	void overlappingOutlinesOnSameSideAreWarned();
	void nonOverlappingOrOppositeSidePlacementsAreNotFlagged();

	void pinOutsideBoardOutlineIsError();
	void pinsAndWiresInsideOutlineAreNotFlagged();

	void bareWiresCrossingOffGridAreError();
	void bareWiresNotCrossingOrCrossingOnGridAreNotFlagged();

	void mismatchedDrillAtSameHoleIsWarned();
	void matchingDrillsAreNotFlagged();

	void floatingFrontWireEndpointOnDoubleSidedBoardIsWarned();
	void frontWireEndpointsAtPinsAreNotFlagged();

private:
	std::unique_ptr<QTemporaryDir> m_appDataDir;
	std::unique_ptr<LibraryManager> m_libMgr;
	QString m_libId = "drclib";

	// 特に指定のないテストでは基板を十分大きくとり、ルール4 (外形の外) が
	// 意図せず誤検出しないようにする。
	static void giveRoomyBoard(Document &doc) {
		doc.board.size = QSize(1000, 1000);
	}
};

void TestDrc::init() {
	m_appDataDir = std::make_unique<QTemporaryDir>();
	qputenv("XDG_DATA_HOME", m_appDataDir->path().toUtf8());
	m_libMgr = std::make_unique<LibraryManager>();
	m_libMgr->loadAll();

	Library lib;
	lib.id = m_libId;
	lib.name = m_libId;
	lib.license.kind = LicenseKind::CC0_1_0;

	// 2ピン部品 (ピン間隔10単位)。
	auto twoPin = makePinPart("TWOPIN", {Pin{1, QPoint(0, 0), 0, {}}, Pin{2, QPoint(10, 0), 0, {}}});
	lib.parts.insert(twoPin->id, twoPin);
	lib.partCategory.insert(twoPin->id, "misc");

	// 単一ピン部品 (共有穴・ドリル不一致のテスト用)。
	auto onePinDrillA = makePinPart("ONEPIN_D10", {Pin{1, QPoint(0, 0), 10, {}}}, PartKind::Normal, QSize(4, 4));
	lib.parts.insert(onePinDrillA->id, onePinDrillA);
	lib.partCategory.insert(onePinDrillA->id, "misc");

	auto onePinDrillB = makePinPart("ONEPIN_D20", {Pin{1, QPoint(0, 0), 20, {}}}, PartKind::Normal, QSize(4, 4));
	lib.parts.insert(onePinDrillB->id, onePinDrillB);
	lib.partCategory.insert(onePinDrillB->id, "misc");

	// スルーホール部品 (番号なしピンだが表裏を強制接続する)。
	auto thru = makePinPart("THRU", {Pin{0, QPoint(0, 0), 0, {}}}, PartKind::ToolThruHole, QSize(4, 4));
	lib.parts.insert(thru->id, thru);
	lib.partCategory.insert(thru->id, "tool");

	const auto result = m_libMgr->installLibrary(lib);
	QVERIFY2(result.ok, qPrintable(result.error));
}

// --- ルール1: 未接続ピン ---

void TestDrc::unconnectedPinIsWarned() {
	Document doc;
	giveRoomyBoard(doc);
	doc.placements.append(place("p1", m_libId, "TWOPIN", QPoint(0, 0)));

	DrcEngine engine;
	const auto findings = engine.run(doc, m_libMgr.get());
	// ピン1(0,0)・ピン2(10,0) はどちらも他と繋がっていないので、それぞれ警告される。
	QCOMPARE(countRule(findings, "unconnected-pin"), 2);
	for (const auto &f : findings) {
		if (f.ruleId == "unconnected-pin") {
			QCOMPARE(static_cast<int>(f.severity), static_cast<int>(DrcSeverity::Warning));
		}
	}
}

void TestDrc::fullyConnectedPinsAreNotWarned() {
	Document doc;
	giveRoomyBoard(doc);
	doc.placements.append(place("p1", m_libId, "TWOPIN", QPoint(0, 0)));
	doc.placements.append(place("p2", m_libId, "TWOPIN", QPoint(30, 0)));
	// (0,0)-(40,0) の直線配線が p1・p2 の4ピン全部の位置を通過するので、全ピンが
	// 1つの大きなネットに属し、どれも「未接続」にはならない。
	doc.wires.append(makeWire("w1", WireLayer::FrontBare, {QPoint(0, 0), QPoint(40, 0)}));

	DrcEngine engine;
	const auto findings = engine.run(doc, m_libMgr.get());
	QVERIFY(!hasRule(findings, "unconnected-pin"));
}

// --- ルール2: 2部品が同じ穴を共有 ---

void TestDrc::sharedHoleIsError() {
	Document doc;
	giveRoomyBoard(doc);
	doc.placements.append(place("p1", m_libId, "ONEPIN_D10", QPoint(50, 50)));
	doc.placements.append(place("p2", m_libId, "ONEPIN_D10", QPoint(50, 50)));  // 同じ穴に重ねて配置

	DrcEngine engine;
	const auto findings = engine.run(doc, m_libMgr.get());
	QVERIFY(hasRule(findings, "shared-hole"));
	for (const auto &f : findings) {
		if (f.ruleId == "shared-hole") {
			QCOMPARE(static_cast<int>(f.severity), static_cast<int>(DrcSeverity::Error));
			QCOMPARE(f.pos, QPoint(50, 50));
		}
	}
}

void TestDrc::distinctHolesAreNotFlagged() {
	Document doc;
	giveRoomyBoard(doc);
	doc.placements.append(place("p1", m_libId, "ONEPIN_D10", QPoint(50, 50)));
	doc.placements.append(place("p2", m_libId, "ONEPIN_D10", QPoint(80, 50)));  // 別の穴

	DrcEngine engine;
	const auto findings = engine.run(doc, m_libMgr.get());
	QVERIFY(!hasRule(findings, "shared-hole"));
}

// --- ルール3: 部品アウトラインの重なり ---

void TestDrc::overlappingOutlinesOnSameSideAreWarned() {
	Document doc;
	giveRoomyBoard(doc);
	doc.placements.append(place("p1", m_libId, "TWOPIN", QPoint(0, 0)));   // (0,0)-(10,10)
	doc.placements.append(place("p2", m_libId, "TWOPIN", QPoint(5, 5)));   // (5,5)-(15,15) 重なる

	DrcEngine engine;
	const auto findings = engine.run(doc, m_libMgr.get());
	QVERIFY(hasRule(findings, "outline-overlap"));
	for (const auto &f : findings) {
		if (f.ruleId == "outline-overlap") {
			QCOMPARE(static_cast<int>(f.severity), static_cast<int>(DrcSeverity::Warning));
		}
	}
}

void TestDrc::nonOverlappingOrOppositeSidePlacementsAreNotFlagged() {
	Document doc;
	giveRoomyBoard(doc);
	doc.placements.append(place("p1", m_libId, "TWOPIN", QPoint(0, 0)));
	doc.placements.append(place("p2", m_libId, "TWOPIN", QPoint(100, 100)));  // 離れている
	doc.placements.append(place("p3", m_libId, "TWOPIN", QPoint(0, 0), Side::Back));  // 完全に重なるが裏面

	DrcEngine engine;
	const auto findings = engine.run(doc, m_libMgr.get());
	QVERIFY(!hasRule(findings, "outline-overlap"));
}

// --- ルール4: ピン/配線が基板外形の外 ---

void TestDrc::pinOutsideBoardOutlineIsError() {
	Document doc;
	doc.board.size = QSize(100, 100);  // x,y ともに [0,99] が範囲内
	doc.placements.append(place("p1", m_libId, "TWOPIN", QPoint(95, 0)));  // ピン2 が (105,0) で範囲外

	DrcEngine engine;
	const auto findings = engine.run(doc, m_libMgr.get());
	QVERIFY(hasRule(findings, "outside-board"));
	for (const auto &f : findings) {
		if (f.ruleId == "outside-board") {
			QCOMPARE(static_cast<int>(f.severity), static_cast<int>(DrcSeverity::Error));
		}
	}
}

void TestDrc::pinsAndWiresInsideOutlineAreNotFlagged() {
	Document doc;
	doc.board.size = QSize(100, 100);
	doc.placements.append(place("p1", m_libId, "TWOPIN", QPoint(0, 0)));  // ピンは (0,0),(10,0) で範囲内
	doc.wires.append(makeWire("w1", WireLayer::BackBare, {QPoint(20, 20), QPoint(40, 20)}));  // 範囲内

	DrcEngine engine;
	const auto findings = engine.run(doc, m_libMgr.get());
	QVERIFY(!hasRule(findings, "outside-board"));
}

// --- ルール5: 同じ面の裸線どうしが格子点以外で交差 ---

void TestDrc::bareWiresCrossingOffGridAreError() {
	Document doc;
	giveRoomyBoard(doc);
	// (0,0)-(10,3) と (0,3)-(10,0) は (5, 1.5) で交差する。y が整数でないため格子点ではない。
	doc.wires.append(makeWire("w1", WireLayer::FrontBare, {QPoint(0, 0), QPoint(10, 3)}));
	doc.wires.append(makeWire("w2", WireLayer::FrontBare, {QPoint(0, 3), QPoint(10, 0)}));

	DrcEngine engine;
	const auto findings = engine.run(doc, m_libMgr.get());
	QVERIFY(hasRule(findings, "wire-short"));
	for (const auto &f : findings) {
		if (f.ruleId == "wire-short") {
			QCOMPARE(static_cast<int>(f.severity), static_cast<int>(DrcSeverity::Error));
		}
	}
}

void TestDrc::bareWiresNotCrossingOrCrossingOnGridAreNotFlagged() {
	Document doc;
	giveRoomyBoard(doc);
	// 交差しない2本。
	doc.wires.append(makeWire("w1", WireLayer::FrontBare, {QPoint(0, 0), QPoint(10, 0)}));
	doc.wires.append(makeWire("w2", WireLayer::FrontBare, {QPoint(0, 100), QPoint(10, 100)}));
	// 格子点 (10,10) でちょうど交差する2本 (正常なT字/十字接続)。
	doc.wires.append(makeWire("w3", WireLayer::FrontBare, {QPoint(0, 10), QPoint(20, 10)}));
	doc.wires.append(makeWire("w4", WireLayer::FrontBare, {QPoint(10, 0), QPoint(10, 20)}));

	DrcEngine engine;
	const auto findings = engine.run(doc, m_libMgr.get());
	QVERIFY(!hasRule(findings, "wire-short"));
}

// --- ルール6: 同じ穴に異なるドリル径のピン ---

void TestDrc::mismatchedDrillAtSameHoleIsWarned() {
	Document doc;
	giveRoomyBoard(doc);
	doc.placements.append(place("p1", m_libId, "ONEPIN_D10", QPoint(60, 60)));
	doc.placements.append(place("p2", m_libId, "ONEPIN_D20", QPoint(60, 60)));  // 同じ穴、ドリル径だけ違う

	DrcEngine engine;
	const auto findings = engine.run(doc, m_libMgr.get());
	QVERIFY(hasRule(findings, "drill-mismatch"));
	for (const auto &f : findings) {
		if (f.ruleId == "drill-mismatch") {
			QCOMPARE(static_cast<int>(f.severity), static_cast<int>(DrcSeverity::Warning));
		}
	}
}

void TestDrc::matchingDrillsAreNotFlagged() {
	Document doc;
	giveRoomyBoard(doc);
	doc.placements.append(place("p1", m_libId, "ONEPIN_D10", QPoint(60, 60)));
	doc.placements.append(place("p2", m_libId, "ONEPIN_D10", QPoint(90, 60)));  // 別の穴、同じドリル径

	DrcEngine engine;
	const auto findings = engine.run(doc, m_libMgr.get());
	QVERIFY(!hasRule(findings, "drill-mismatch"));
}

// --- ルール7: 両面基板で表面配線の端点にピン/スルーホールが無い ---

void TestDrc::floatingFrontWireEndpointOnDoubleSidedBoardIsWarned() {
	Document doc;
	giveRoomyBoard(doc);
	doc.board.doubleSided = true;
	// 端点 (0,0),(20,0) のどちらにもピンが無い。
	doc.wires.append(makeWire("w1", WireLayer::FrontBare, {QPoint(0, 0), QPoint(20, 0)}));

	DrcEngine engine;
	const auto findings = engine.run(doc, m_libMgr.get());
	QCOMPARE(countRule(findings, "front-wire-floating-endpoint"), 2);
	for (const auto &f : findings) {
		if (f.ruleId == "front-wire-floating-endpoint") {
			QCOMPARE(static_cast<int>(f.severity), static_cast<int>(DrcSeverity::Warning));
		}
	}
}

void TestDrc::frontWireEndpointsAtPinsAreNotFlagged() {
	Document doc;
	giveRoomyBoard(doc);
	doc.board.doubleSided = true;
	doc.placements.append(place("p1", m_libId, "ONEPIN_D10", QPoint(0, 0)));
	doc.placements.append(place("p2", m_libId, "ONEPIN_D10", QPoint(20, 0)));
	doc.wires.append(makeWire("w1", WireLayer::FrontBare, {QPoint(0, 0), QPoint(20, 0)}));

	DrcEngine engine;
	const auto findings = engine.run(doc, m_libMgr.get());
	QVERIFY(!hasRule(findings, "front-wire-floating-endpoint"));

	// 片面基板であればそもそもこのルールは働かない (端点にピンが無くても無警告)。
	Document singleSided;
	giveRoomyBoard(singleSided);
	singleSided.board.doubleSided = false;
	singleSided.wires.append(makeWire("w1", WireLayer::FrontBare, {QPoint(0, 0), QPoint(20, 0)}));
	const auto findings2 = engine.run(singleSided, m_libMgr.get());
	QVERIFY(!hasRule(findings2, "front-wire-floating-endpoint"));
}

QTEST_MAIN(TestDrc)
#include "test_drc.moc"
