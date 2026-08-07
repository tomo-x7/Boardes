#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QTest>
#include <QTextStream>
#include <algorithm>

#include "core/units.h"
#include "model/document.h"
#include "model/librarymanager.h"
#include "model/stats.h"

namespace {

std::shared_ptr<Part> makePart(const QString &id, QVector<Pin> pins, PartKind kind = PartKind::Normal,
							   QSize size = QSize(10, 10)) {
	auto part = std::make_shared<Part>();
	part->id = id;
	part->name = QStringLiteral("部品-") + id;
	part->kind = kind;
	part->outline = QRect(QPoint(0, 0), size);
	QImage img(size, QImage::Format_RGB888);
	img.fill(Qt::gray);
	part->artwork = Artwork::fromImageAsIs(img);
	part->pins = std::move(pins);
	return part;
}

std::shared_ptr<Placement> place(const QString &uuid, const QString &libId, const QString &partId, QPoint pos,
								 const QString &refDes = QString(), const QString &value = QString()) {
	auto p = std::make_shared<Placement>();
	p->uuid = uuid;
	p->libraryId = libId;
	p->partId = partId;
	p->pos = pos;
	p->refDes = refDes;
	p->value = value;
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

class TestStats : public QObject {
	Q_OBJECT

private slots:
	void init();

	void orthogonalWireLengthMatchesSegmentMm();
	void diagonalWireLengthMatchesDiagonalMm();
	void halfPitchWireLengthIsHalf();
	void wireLengthIsBrokenDownByLayer();

	void holeCountSumsPinsEndpointsAndThruHoles();
	void thruHolePinsAreNotDoubleCountedAsComponentPins();

	void areaAndOccupancyRatioAreComputed();

	void bomGroupsByPartAndValueWithJoinedRefDes();
	void bomCsvRoundTripsThroughFile();

private:
	std::unique_ptr<QTemporaryDir> m_appDataDir;
	std::unique_ptr<LibraryManager> m_libMgr;
	QString m_libId = "statslib";
};

void TestStats::init() {
	m_appDataDir = std::make_unique<QTemporaryDir>();
	QVERIFY(m_appDataDir->isValid());
	qputenv("XDG_DATA_HOME", m_appDataDir->path().toUtf8());
	m_libMgr = std::make_unique<LibraryManager>();
	m_libMgr->loadAll();

	Library lib;
	lib.id = m_libId;
	lib.name = QStringLiteral("統計用ライブラリ");
	lib.license.kind = LicenseKind::CC0_1_0;

	auto r = makePart("R", {Pin{1, QPoint(0, 0), 0, {}}, Pin{2, QPoint(10, 0), 0, {}}});
	lib.parts.insert(r->id, r);
	lib.partCategory.insert(r->id, "misc");

	auto thru = makePart("THRU", {Pin{0, QPoint(0, 0), 0, {}}}, PartKind::ToolThruHole, QSize(4, 4));
	lib.parts.insert(thru->id, thru);
	lib.partCategory.insert(thru->id, "tool");

	const auto result = m_libMgr->installLibrary(lib);
	QVERIFY2(result.ok, qPrintable(result.error));
}

void TestStats::orthogonalWireLengthMatchesSegmentMm() {
	Document doc;
	doc.wires.append(makeWire("w1", WireLayer::FrontBare, {QPoint(0, 0), QPoint(10, 0)}));  // フル1区間、直交

	StatsEngine engine;
	const auto stats = engine.compute(doc, m_libMgr.get());
	QVERIFY(qFuzzyCompare(stats.totalWireLengthMm, units::SegmentMm));
}

void TestStats::diagonalWireLengthMatchesDiagonalMm() {
	Document doc;
	doc.wires.append(makeWire("w1", WireLayer::FrontBare, {QPoint(0, 0), QPoint(10, 10)}));  // フル1区間、45度

	StatsEngine engine;
	const auto stats = engine.compute(doc, m_libMgr.get());
	QVERIFY(qFuzzyCompare(stats.totalWireLengthMm, units::DiagonalMm));
}

void TestStats::halfPitchWireLengthIsHalf() {
	Document doc;
	doc.wires.append(makeWire("w1", WireLayer::FrontBare, {QPoint(0, 0), QPoint(5, 0)}));   // ハーフ直交
	doc.wires.append(makeWire("w2", WireLayer::BackBare, {QPoint(0, 0), QPoint(5, 5)}));    // ハーフ45度

	StatsEngine engine;
	const auto stats = engine.compute(doc, m_libMgr.get());
	QVERIFY(qFuzzyCompare(stats.wireLengthMmByLayer.value(WireLayer::FrontBare), units::SegmentMm / 2.0));
	QVERIFY(qFuzzyCompare(stats.wireLengthMmByLayer.value(WireLayer::BackBare), units::DiagonalMm / 2.0));
}

void TestStats::wireLengthIsBrokenDownByLayer() {
	Document doc;
	doc.wires.append(makeWire("w1", WireLayer::FrontBare, {QPoint(0, 0), QPoint(10, 0)}));
	doc.wires.append(makeWire("w2", WireLayer::FrontBare, {QPoint(0, 0), QPoint(20, 0)}));  // 2区間分
	doc.wires.append(makeWire("w3", WireLayer::BackInsulated, {QPoint(0, 0), QPoint(10, 0)}));

	StatsEngine engine;
	const auto stats = engine.compute(doc, m_libMgr.get());
	QVERIFY(qFuzzyCompare(stats.wireLengthMmByLayer.value(WireLayer::FrontBare), units::SegmentMm * 3.0));
	QVERIFY(qFuzzyCompare(stats.wireLengthMmByLayer.value(WireLayer::BackInsulated), units::SegmentMm));
	QVERIFY(qFuzzyCompare(stats.totalWireLengthMm, units::SegmentMm * 4.0));
	QCOMPARE(stats.wireCount, 3);
}

void TestStats::holeCountSumsPinsEndpointsAndThruHoles() {
	Document doc;
	doc.placements.append(place("p1", m_libId, "R", QPoint(0, 0)));   // ピン2個
	doc.placements.append(place("p2", m_libId, "THRU", QPoint(50, 50)));  // スルーホール1個
	doc.wires.append(makeWire("w1", WireLayer::FrontBare, {QPoint(0, 0), QPoint(10, 0)}));  // 端点2個

	StatsEngine engine;
	const auto stats = engine.compute(doc, m_libMgr.get());
	QCOMPARE(stats.componentPinHoleCount, 2);
	QCOMPARE(stats.thruHoleCount, 1);
	QCOMPARE(stats.frontBareWireEndpointCount, 2);
	QCOMPARE(stats.totalHoleCount, 5);
}

void TestStats::thruHolePinsAreNotDoubleCountedAsComponentPins() {
	Document doc;
	doc.placements.append(place("p1", m_libId, "THRU", QPoint(0, 0)));

	StatsEngine engine;
	const auto stats = engine.compute(doc, m_libMgr.get());
	QCOMPARE(stats.thruHoleCount, 1);
	QCOMPARE(stats.componentPinHoleCount, 0);  // THRU のピンは componentPinHoleCount に含めない
	QCOMPARE(stats.totalHoleCount, 1);
}

void TestStats::areaAndOccupancyRatioAreComputed() {
	Document doc;
	doc.board.size = QSize(100, 100);  // (100*0.254)^2 mm^2
	doc.placements.append(place("p1", m_libId, "R", QPoint(0, 0)));  // 10x10 単位

	StatsEngine engine;
	const auto stats = engine.compute(doc, m_libMgr.get());
	const double expectedBoardArea = (100 * units::MmPerUnit) * (100 * units::MmPerUnit);
	const double expectedOccupied = (10 * units::MmPerUnit) * (10 * units::MmPerUnit);
	QVERIFY(qFuzzyCompare(stats.boardAreaMm2, expectedBoardArea));
	QVERIFY(qFuzzyCompare(stats.occupiedAreaMm2, expectedOccupied));
	QVERIFY(qFuzzyCompare(stats.occupancyRatio, expectedOccupied / expectedBoardArea));
}

void TestStats::bomGroupsByPartAndValueWithJoinedRefDes() {
	Document doc;
	doc.placements.append(place("p1", m_libId, "R", QPoint(0, 0), "R1", "10k"));
	doc.placements.append(place("p2", m_libId, "R", QPoint(20, 0), "R2", "10k"));   // R1と同じグループ
	doc.placements.append(place("p3", m_libId, "R", QPoint(40, 0), "R3", "4.7k"));  // 値違いで別グループ

	StatsEngine engine;
	const auto bom = engine.computeBom(doc, m_libMgr.get());
	QCOMPARE(bom.size(), 2);

	const auto it10k = std::find_if(bom.begin(), bom.end(), [](const BomRow &r) { return r.value == "10k"; });
	QVERIFY(it10k != bom.end());
	QCOMPARE(it10k->quantity, 2);
	QVERIFY(it10k->refDesList.contains("R1"));
	QVERIFY(it10k->refDesList.contains("R2"));
	QCOMPARE(it10k->partName, QStringLiteral("部品-R"));
	QCOMPARE(it10k->libraryName, QStringLiteral("統計用ライブラリ"));

	const auto it47k = std::find_if(bom.begin(), bom.end(), [](const BomRow &r) { return r.value == "4.7k"; });
	QVERIFY(it47k != bom.end());
	QCOMPARE(it47k->quantity, 1);
	QCOMPARE(it47k->refDesList, QStringLiteral("R3"));
}

void TestStats::bomCsvRoundTripsThroughFile() {
	Document doc;
	doc.placements.append(place("p1", m_libId, "R", QPoint(0, 0), "R1", "10k"));
	doc.placements.append(place("p2", m_libId, "R", QPoint(20, 0), "R2", "10k"));

	StatsEngine engine;
	const auto bom = engine.computeBom(doc, m_libMgr.get());

	const QString csvPath = m_appDataDir->filePath("bom.csv");
	QVERIFY(StatsEngine::saveBomCsv(bom, csvPath));

	QFile file(csvPath);
	QVERIFY(file.open(QIODevice::ReadOnly));
	const QByteArray bytes = file.readAll();
	// UTF-8 BOM (EF BB BF) が先頭に付与されていること。
	QCOMPARE(bytes.left(3), QByteArray("\xEF\xBB\xBF", 3));

	file.seek(0);
	QTextStream reader(&file);
	reader.setEncoding(QStringConverter::Utf8);
	const QString content = reader.readAll();
	QVERIFY(content.contains(QStringLiteral("refDes,部品名,値,ライブラリ,数量")));
	QVERIFY(content.contains(QStringLiteral("R1, R2")));
	QVERIFY(content.contains(QStringLiteral("10k")));
	QVERIFY(content.contains(QStringLiteral("2")));  // 数量
}

QTEST_MAIN(TestStats)
#include "test_stats.moc"
