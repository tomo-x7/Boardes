#include <QImage>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>

#include "model/document.h"
#include "model/librarymanager.h"
#include "ui/drcpanel.h"

// DrcPanel は「実際に一覧が埋まり、行クリックで正しい DrcFinding が飛んでくる」ところまでを
// 本物の QTreeWidget に対する QTest::mouseClick で検証する (信号配線だけのテストだと
// 「クリックしても何も起きない」ような結線ミスを見逃すため)。
class TestDrcPanel : public QObject {
	Q_OBJECT

private slots:
	void init();
	void cleanup();

	void emptyDocumentShowsNoFindings();
	void findingIsListedAndClickEmitsIt();

private:
	std::unique_ptr<QTemporaryDir> m_appDataDir;
	std::unique_ptr<LibraryManager> m_libMgr;
	std::unique_ptr<Document> m_doc;
	QString m_libId = "drcpanellib";
};

void TestDrcPanel::init() {
	m_appDataDir = std::make_unique<QTemporaryDir>();
	QVERIFY(m_appDataDir->isValid());
	qputenv("XDG_DATA_HOME", m_appDataDir->path().toUtf8());
	m_libMgr = std::make_unique<LibraryManager>();
	m_libMgr->loadAll();
	m_doc = std::make_unique<Document>();
	m_doc->board.size = QSize(1000, 1000);  // ルール4 (外形の外) の誤検出を避ける
}

void TestDrcPanel::cleanup() {
	m_doc.reset();
	m_libMgr.reset();
	m_appDataDir.reset();
}

void TestDrcPanel::emptyDocumentShowsNoFindings() {
	DrcPanel panel;
	panel.setContext(m_doc.get(), m_libMgr.get());

	auto *tree = panel.findChild<QTreeWidget *>();
	QVERIFY(tree != nullptr);
	QCOMPARE(tree->topLevelItemCount(), 0);
}

void TestDrcPanel::findingIsListedAndClickEmitsIt() {
	Library lib;
	lib.id = m_libId;
	lib.name = m_libId;
	lib.license.kind = LicenseKind::CC0_1_0;

	auto part = std::make_shared<Part>();
	part->id = "P1";
	part->name = "P1";
	QImage img(10, 10, QImage::Format_RGB888);
	img.fill(Qt::gray);
	part->artwork = Artwork::fromImageAsIs(img);
	part->pins = {Pin{1, QPoint(0, 0), 0, {}}};  // 何にも繋がっていない番号付きピン
	lib.parts.insert(part->id, part);
	lib.partCategory.insert(part->id, "misc");
	QVERIFY(m_libMgr->installLibrary(lib).ok);

	auto placement = std::make_shared<Placement>();
	placement->uuid = "p1";
	placement->libraryId = m_libId;
	placement->partId = "P1";
	placement->pos = QPoint(50, 50);
	m_doc->placements.append(placement);

	DrcPanel panel;
	panel.resize(400, 300);
	panel.setContext(m_doc.get(), m_libMgr.get());

	auto *tree = panel.findChild<QTreeWidget *>();
	QVERIFY(tree != nullptr);
	QCOMPARE(tree->topLevelItemCount(), 1);  // 「未接続ピン」が1件だけ検出される

	DrcFinding captured;
	bool gotSignal = false;
	connect(&panel, &DrcPanel::findingActivated, &panel,
			[&](const DrcFinding &f) {
				captured = f;
				gotSignal = true;
			});

	panel.show();
	QVERIFY(QTest::qWaitForWindowExposed(&panel));
	QTreeWidgetItem *item = tree->topLevelItem(0);
	QVERIFY(item != nullptr);
	const QRect rect = tree->visualItemRect(item);
	QVERIFY(!rect.isEmpty());
	QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, rect.center());

	QVERIFY(gotSignal);
	QCOMPARE(captured.ruleId, QStringLiteral("unconnected-pin"));
	QCOMPARE(captured.pos, QPoint(50, 50));
	QCOMPARE(captured.relatedPlacementUuid, QStringLiteral("p1"));
}

QTEST_MAIN(TestDrcPanel)
#include "test_drcpanel.moc"
