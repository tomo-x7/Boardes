#include <QImage>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTest>

#include "model/document.h"
#include "model/librarymanager.h"
#include "ui/statspanel.h"

// StatsPanel は「実際に BOM 表が埋まる」ところまでを本物のウィジェットで検証する。
// CSV 書き出しボタン自体は QFileDialog を開くため QTest では直接押さないが、
// StatsPanel が使う StatsEngine::saveBomCsv の経路は test_stats.cpp 側で別途検証済み。
class TestStatsPanel : public QObject {
	Q_OBJECT

private slots:
	void init();
	void cleanup();

	void emptyDocumentShowsZeroCounts();
	void bomTableIsPopulatedFromPlacements();

private:
	std::unique_ptr<QTemporaryDir> m_appDataDir;
	std::unique_ptr<LibraryManager> m_libMgr;
	std::unique_ptr<Document> m_doc;
	QString m_libId = "statspanellib";
};

void TestStatsPanel::init() {
	m_appDataDir = std::make_unique<QTemporaryDir>();
	QVERIFY(m_appDataDir->isValid());
	qputenv("XDG_DATA_HOME", m_appDataDir->path().toUtf8());
	m_libMgr = std::make_unique<LibraryManager>();
	m_libMgr->loadAll();
	m_doc = std::make_unique<Document>();
	m_doc->board.size = QSize(200, 200);
}

void TestStatsPanel::cleanup() {
	m_doc.reset();
	m_libMgr.reset();
	m_appDataDir.reset();
}

void TestStatsPanel::emptyDocumentShowsZeroCounts() {
	StatsPanel panel;
	panel.setContext(m_doc.get(), m_libMgr.get());

	auto *table = panel.findChild<QTableWidget *>();
	QVERIFY(table != nullptr);
	QCOMPARE(table->rowCount(), 0);
}

void TestStatsPanel::bomTableIsPopulatedFromPlacements() {
	Library lib;
	lib.id = m_libId;
	lib.name = QStringLiteral("パネル用ライブラリ");
	lib.license.kind = LicenseKind::CC0_1_0;

	auto part = std::make_shared<Part>();
	part->id = "R";
	part->name = QStringLiteral("抵抗");
	QImage img(10, 10, QImage::Format_RGB888);
	img.fill(Qt::gray);
	part->artwork = Artwork::fromImageAsIs(img);
	part->pins = {Pin{1, QPoint(0, 0), 0, {}}, Pin{2, QPoint(10, 0), 0, {}}};
	lib.parts.insert(part->id, part);
	lib.partCategory.insert(part->id, "misc");
	QVERIFY(m_libMgr->installLibrary(lib).ok);

	auto p1 = std::make_shared<Placement>();
	p1->uuid = "p1";
	p1->libraryId = m_libId;
	p1->partId = "R";
	p1->pos = QPoint(0, 0);
	p1->refDes = "R1";
	p1->value = "10k";
	auto p2 = std::make_shared<Placement>();
	p2->uuid = "p2";
	p2->libraryId = m_libId;
	p2->partId = "R";
	p2->pos = QPoint(30, 0);
	p2->refDes = "R2";
	p2->value = "10k";
	m_doc->placements = {p1, p2};

	StatsPanel panel;
	panel.setContext(m_doc.get(), m_libMgr.get());

	auto *table = panel.findChild<QTableWidget *>();
	QVERIFY(table != nullptr);
	QCOMPARE(table->rowCount(), 1);  // 同じ部品+値なので1行にまとまる
	QCOMPARE(table->item(0, 0)->text(), QStringLiteral("R1, R2"));
	QCOMPARE(table->item(0, 1)->text(), QStringLiteral("抵抗"));
	QCOMPARE(table->item(0, 2)->text(), QStringLiteral("10k"));
	QCOMPARE(table->item(0, 4)->text(), QStringLiteral("2"));

	// refresh() を呼び直しても (増分ではなく) 正しく再構築されること。
	m_doc->placements.removeLast();
	panel.refresh();
	QCOMPARE(table->rowCount(), 1);
	QCOMPARE(table->item(0, 4)->text(), QStringLiteral("1"));
}

QTEST_MAIN(TestStatsPanel)
#include "test_statspanel.moc"
