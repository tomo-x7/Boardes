#include <QImage>
#include <QTemporaryDir>
#include <QTest>

#include "model/document.h"
#include "model/librarymanager.h"
#include "model/netlist.h"

namespace {

std::shared_ptr<Part> makePinPart(const QString &id, QVector<Pin> pins, PartKind kind = PartKind::Normal) {
	auto part = std::make_shared<Part>();
	part->id = id;
	part->name = id;
	part->kind = kind;
	part->outline = QRect(0, 0, 10, 10);
	QImage img(10, 10, QImage::Format_RGB888);
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
	return p;
}

std::shared_ptr<Wire> makeWire(const QString &uuid, WireLayer layer, QVector<QPoint> points) {
	auto w = std::make_shared<Wire>();
	w->uuid = uuid;
	w->layer = layer;
	w->points = std::move(points);
	return w;
}

}  // namespace

class TestNetlist : public QObject {
	Q_OBJECT

private slots:
	void init();

	void twoPinPartFootHasBothSidesConnected();
	void frontBareWireConnectsPinsAlongItsPath();
	void bareWiresTouchingAtIntermediatePointAreSameNet();
	void insulatedWiresCrossingAtMidpointStayIsolated();
	void singleSidedBoardBridgesFrontWireEndpointsToBack();
	void doubleSidedBoardDoesNotAutoBridgeFrontWire();
	void thruHoleBridgesRegardlessOfBoardSidedness();
	void unconnectedPinFormsItsOwnNet();

private:
	std::unique_ptr<QTemporaryDir> m_appDataDir;
	std::unique_ptr<LibraryManager> m_libMgr;
	QString m_libId;
};

void TestNetlist::init() {
	m_appDataDir = std::make_unique<QTemporaryDir>();
	qputenv("XDG_DATA_HOME", m_appDataDir->path().toUtf8());
	m_libMgr = std::make_unique<LibraryManager>();
	m_libMgr->loadAll();

	Library lib;
	lib.id = "netlib";
	lib.name = "netlib";
	lib.license.kind = LicenseKind::CC0_1_0;

	// 2ピンの部品 (ジャンパ線的な footprint、ピン間隔10単位)。
	auto twoPin = makePinPart("TWOPIN", {Pin{1, QPoint(0, 0), 0, {}}, Pin{2, QPoint(10, 0), 0, {}}});
	lib.parts.insert(twoPin->id, twoPin);
	lib.partCategory.insert(twoPin->id, "misc");

	// スルーホール部品 (1ピン、表裏を強制的に接続する)。
	auto thru = makePinPart("THRU", {Pin{0, QPoint(0, 0), 0, {}}}, PartKind::ToolThruHole);
	lib.parts.insert(thru->id, thru);
	lib.partCategory.insert(thru->id, "tool");

	const auto result = m_libMgr->installLibrary(lib);
	QVERIFY2(result.ok, qPrintable(result.error));
	m_libId = "netlib";
}

void TestNetlist::twoPinPartFootHasBothSidesConnected() {
	Document doc;
	doc.placements.append(place("p1", m_libId, "TWOPIN", QPoint(0, 0)));

	Netlist net;
	net.rebuild(doc, m_libMgr.get());

	// 部品の足 (ピン1の位置) は表裏で同一ネット。
	QVERIFY(net.sameNet(QPoint(0, 0), Side::Front, QPoint(0, 0), Side::Back));
	// ピン1とピン2は互いに未接続。
	QVERIFY(!net.sameNet(QPoint(0, 0), Side::Front, QPoint(10, 0), Side::Front));
}

void TestNetlist::frontBareWireConnectsPinsAlongItsPath() {
	Document doc;
	doc.board.doubleSided = true;
	doc.placements.append(place("p1", m_libId, "TWOPIN", QPoint(0, 0)));
	doc.placements.append(place("p2", m_libId, "TWOPIN", QPoint(20, 0)));
	// p1のピン2(10,0) から p2のピン1(20,0) へ配線。
	doc.wires.append(makeWire("w1", WireLayer::FrontBare, {QPoint(10, 0), QPoint(20, 0)}));

	Netlist net;
	net.rebuild(doc, m_libMgr.get());
	QVERIFY(net.sameNet(QPoint(10, 0), Side::Front, QPoint(20, 0), Side::Front));
}

void TestNetlist::bareWiresTouchingAtIntermediatePointAreSameNet() {
	Document doc;
	doc.board.doubleSided = true;
	// 1本目: (0,0)->(30,0) の水平配線 (途中に(10,0)を通過)。
	doc.wires.append(makeWire("w1", WireLayer::FrontBare, {QPoint(0, 0), QPoint(30, 0)}));
	// 2本目: (10,0)->(10,20) は1本目の経路の途中 (10,0) から分岐。
	doc.wires.append(makeWire("w2", WireLayer::FrontBare, {QPoint(10, 0), QPoint(10, 20)}));

	Netlist net;
	net.rebuild(doc, m_libMgr.get());
	// 触れているので裸線同士は同一ネットになる。
	QVERIFY(net.sameNet(QPoint(0, 0), Side::Front, QPoint(10, 20), Side::Front));
}

void TestNetlist::insulatedWiresCrossingAtMidpointStayIsolated() {
	Document doc;
	doc.board.doubleSided = true;
	// 被覆配線は端点のみが接続対象なので、(10,0)を「通過」しても分岐先とは繋がらない。
	doc.wires.append(makeWire("w1", WireLayer::FrontInsulated, {QPoint(0, 0), QPoint(30, 0)}));
	doc.wires.append(makeWire("w2", WireLayer::FrontInsulated, {QPoint(10, 0), QPoint(10, 20)}));

	Netlist net;
	net.rebuild(doc, m_libMgr.get());
	QVERIFY(!net.sameNet(QPoint(0, 0), Side::Front, QPoint(10, 20), Side::Front));
	// w1自体の両端は当然繋がっている。
	QVERIFY(net.sameNet(QPoint(0, 0), Side::Front, QPoint(30, 0), Side::Front));
}

void TestNetlist::singleSidedBoardBridgesFrontWireEndpointsToBack() {
	Document doc;
	doc.board.doubleSided = false;  // 片面基板
	doc.wires.append(makeWire("w1", WireLayer::FrontBare, {QPoint(0, 0), QPoint(10, 0)}));

	Netlist net;
	net.rebuild(doc, m_libMgr.get());
	QVERIFY(net.sameNet(QPoint(0, 0), Side::Front, QPoint(0, 0), Side::Back));
	QVERIFY(net.sameNet(QPoint(10, 0), Side::Front, QPoint(10, 0), Side::Back));
}

void TestNetlist::doubleSidedBoardDoesNotAutoBridgeFrontWire() {
	Document doc;
	doc.board.doubleSided = true;
	doc.wires.append(makeWire("w1", WireLayer::FrontBare, {QPoint(0, 0), QPoint(10, 0)}));

	Netlist net;
	net.rebuild(doc, m_libMgr.get());
	QVERIFY(!net.sameNet(QPoint(0, 0), Side::Front, QPoint(0, 0), Side::Back));
}

void TestNetlist::thruHoleBridgesRegardlessOfBoardSidedness() {
	Document doc;
	doc.board.doubleSided = true;  // 両面基板でも
	doc.placements.append(place("t1", m_libId, "THRU", QPoint(5, 5)));

	Netlist net;
	net.rebuild(doc, m_libMgr.get());
	QVERIFY(net.sameNet(QPoint(5, 5), Side::Front, QPoint(5, 5), Side::Back));
}

void TestNetlist::unconnectedPinFormsItsOwnNet() {
	Document doc;
	doc.placements.append(place("p1", m_libId, "TWOPIN", QPoint(0, 0)));

	Netlist net;
	net.rebuild(doc, m_libMgr.get());
	// ピン2はどの配線にも繋がっていないが、番号付きピンの足は必ず表裏を貫通するので
	// そのネットは {Front(10,0), Back(10,0)} の2ノードだけになる (孤立ネット)。
	const int netId = net.netIdAt(QPoint(10, 0), Side::Front);
	QVERIFY(netId >= 0);
	const auto nodes = net.nodesInNet(netId);
	QCOMPARE(nodes.size(), 2);
	QVERIFY(net.sameNet(QPoint(10, 0), Side::Front, QPoint(10, 0), Side::Back));
	// ピン1のネットとは別。
	QVERIFY(net.netIdAt(QPoint(0, 0), Side::Front) != netId);
}

QTEST_MAIN(TestNetlist)
#include "test_netlist.moc"
