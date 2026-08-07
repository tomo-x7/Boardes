#include <QImage>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include "io/boardio.h"
#include "io/libraryio.h"
#include "io/partio.h"
#include "model/librarymanager.h"

namespace {

Part makeTestPart(const QString &id) {
	Part part;
	part.id = id;
	part.name = id;
	part.refPrefix = QStringLiteral("R");
	QImage img(4, 2, QImage::Format_RGB888);
	img.fill(Qt::blue);
	part.artwork = Artwork::fromImageAsIs(img);
	part.pins = {Pin{1, QPoint(0, 0), 0, QString()}};
	return part;
}

BoardSpec makeTestBoard(const QString &id) {
	BoardSpec b;
	b.id = id;
	b.name = id;
	b.size = QSize(50, 40);
	b.cols = 4;
	b.rows = 3;
	b.pitch = 10;
	b.origin = QPoint(5, 5);
	return b;
}

}  // namespace

class TestLibraryManager : public QObject {
	Q_OBJECT

private slots:
	void initTestCase();
	void init();

	void loadAllCreatesBuiltinLibraries();
	void importPassFolderPopulatesPassCompat();
	void importPartFileAddsToMyLibraryUncategorized();
	void installBlibCreatesReadOnlyIndependentLibrary();
	void installBlibRejectsReservedIds();
	void installLibraryIsIdempotentWhenAlreadyInstalled();
	void duplicateLibraryAppliesNewMetadataAndRedistributionRule();
	void duplicateLibraryRejectsIdCollision();
	void removeLibraryRefusesBuiltins();
	void exportLibraryRespectsRedistributionFlag();
	void persistedLibrariesSurviveReload();

	void addPartToMyLibraryInsertsAndPersists();
	void addBoardToMyLibraryInsertsAndPersists();
	void uniqueIdHelpersAvoidCollisionsInMyLibrary();
	void updateLibraryMetadataRefusesReadOnlyLibraries();
	void updateLibraryMetadataUpdatesFieldsButKeepsIdAndContents();

private:
	std::unique_ptr<QTemporaryDir> m_home;
};

void TestLibraryManager::initTestCase() {
}

void TestLibraryManager::init() {
	// QStandardPaths::AppDataLocation (Linux では $XDG_DATA_HOME 由来) をテストごとに
	// 独立した一時ディレクトリへ差し替え、実ユーザーの環境やテスト間の状態共有を防ぐ。
	m_home = std::make_unique<QTemporaryDir>();
	QVERIFY(m_home->isValid());
	qputenv("XDG_DATA_HOME", m_home->path().toUtf8());
}

void TestLibraryManager::loadAllCreatesBuiltinLibraries() {
	LibraryManager mgr;
	mgr.loadAll();

	const auto myLib = mgr.library(LibraryManager::myLibraryId());
	QVERIFY(myLib != nullptr);
	QVERIFY(!myLib->readOnly);
	QVERIFY(!myLib->redistribution.allowed);

	const auto passLib = mgr.library(LibraryManager::passCompatId());
	QVERIFY(passLib != nullptr);
	QVERIFY(passLib->readOnly);
	QVERIFY(!passLib->redistribution.allowed);
}

void TestLibraryManager::importPassFolderPopulatesPassCompat() {
	QTemporaryDir src;
	QVERIFY(src.isValid());
	QVERIFY(QDir(src.path()).mkpath("R"));
	QImage img(10, 5, QImage::Format_RGB888);
	img.fill(QColor(187, 201, 158));
	img.setPixelColor(2, 2, QColor(254, 0, 0));
	img.save(src.path() + "/R/R-1.bmp", "BMP");

	LibraryManager mgr;
	mgr.loadAll();
	const auto result = mgr.importPassFolder(src.path());
	QVERIFY2(result.ok, qPrintable(result.error));
	QCOMPARE(result.libraryId, LibraryManager::passCompatId());
	QCOMPARE(result.partCount, 1);

	const auto lib = mgr.library(LibraryManager::passCompatId());
	QVERIFY(lib->parts.contains("R-1"));
	QVERIFY(mgr.resolvePart(LibraryManager::passCompatId(), "R-1") != nullptr);
}

void TestLibraryManager::importPartFileAddsToMyLibraryUncategorized() {
	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	const Part part = makeTestPart("MyPart");
	const QString path = dir.filePath("MyPart.bpart");
	QVERIFY(partio::saveEmbedded(part, path));

	LibraryManager mgr;
	mgr.loadAll();
	const auto result = mgr.importPartFile(path);
	QVERIFY2(result.ok, qPrintable(result.error));
	QCOMPARE(result.libraryId, LibraryManager::myLibraryId());

	const auto lib = mgr.library(LibraryManager::myLibraryId());
	QVERIFY(lib->parts.contains("MyPart"));
	QCOMPARE(lib->partCategory.value("MyPart"), LibraryManager::uncategorizedCategoryId());
}

void TestLibraryManager::installBlibCreatesReadOnlyIndependentLibrary() {
	Library lib;
	lib.id = QStringLiteral("third-party-lib");
	lib.name = QStringLiteral("サードパーティ");
	lib.license.kind = LicenseKind::MIT;
	lib.redistribution = redistributionRuleFor(LicenseKind::MIT);
	lib.readOnly = false;  // 元は編集可能だったとしても、インストール後は編集不可にする
	auto part = std::make_shared<Part>(makeTestPart("TP-1"));
	lib.parts.insert(part->id, part);
	lib.partCategory.insert(part->id, "misc");

	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	const QString blibPath = dir.filePath("third-party.blib");
	QVERIFY(libraryio::exportToBlib(lib, blibPath));

	LibraryManager mgr;
	mgr.loadAll();
	const auto result = mgr.installBlib(blibPath);
	QVERIFY2(result.ok, qPrintable(result.error));
	QCOMPARE(result.libraryId, QStringLiteral("third-party-lib"));

	const auto installed = mgr.library("third-party-lib");
	QVERIFY(installed != nullptr);
	QVERIFY(installed->readOnly);
	QVERIFY(installed->parts.contains("TP-1"));
}

void TestLibraryManager::installBlibRejectsReservedIds() {
	Library lib;
	lib.id = LibraryManager::myLibraryId();  // 予約済み id を騙る不正な .blib
	lib.name = QStringLiteral("なりすまし");

	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	const QString blibPath = dir.filePath("bad.blib");
	QVERIFY(libraryio::exportToBlib(lib, blibPath));

	LibraryManager mgr;
	mgr.loadAll();
	const auto result = mgr.installBlib(blibPath);
	QVERIFY(!result.ok);
}

void TestLibraryManager::installLibraryIsIdempotentWhenAlreadyInstalled() {
	Library lib;
	lib.id = QStringLiteral("direct-install-lib");
	lib.name = QStringLiteral("直接インストール");
	lib.license.kind = LicenseKind::MIT;
	lib.redistribution = redistributionRuleFor(LicenseKind::MIT);
	auto part = std::make_shared<Part>(makeTestPart("DP-1"));
	lib.parts.insert(part->id, part);

	LibraryManager mgr;
	mgr.loadAll();

	const auto first = mgr.installLibrary(lib);
	QVERIFY2(first.ok, qPrintable(first.error));
	QCOMPARE(first.partCount, 1);
	QVERIFY(mgr.library("direct-install-lib")->readOnly);

	// 同じ id を再インストールしても失敗せず、既存の内容を上書きしない。
	Library changedLib = lib;
	changedLib.name = QStringLiteral("差し替えを試みる名前");
	const auto second = mgr.installLibrary(changedLib);
	QVERIFY(second.ok);
	QCOMPARE(mgr.library("direct-install-lib")->name, QStringLiteral("直接インストール"));
}

void TestLibraryManager::duplicateLibraryAppliesNewMetadataAndRedistributionRule() {
	LibraryManager mgr;
	mgr.loadAll();

	LibraryManager::DuplicateSpec spec;
	spec.newId = QStringLiteral("my-pass-fork");
	spec.newName = QStringLiteral("PasS互換 (改変版)");
	spec.newAuthor = QStringLiteral("tomo-x");
	spec.newVersion = QStringLiteral("1.0.0");
	spec.newLicense.kind = LicenseKind::CC_BY_SA_4_0;

	const auto result = mgr.duplicateLibrary(LibraryManager::passCompatId(), spec);
	QVERIFY2(result.ok, qPrintable(result.error));

	const auto dup = mgr.library("my-pass-fork");
	QVERIFY(dup != nullptr);
	QVERIFY(!dup->readOnly);
	QVERIFY(dup->redistribution.allowed);
	QCOMPARE(static_cast<int>(dup->redistribution.derivativePolicy), static_cast<int>(DerivativePolicy::MustMatchSame));
	QVERIFY(dup->basedOn.has_value());
	QCOMPARE(dup->basedOn->libraryId, LibraryManager::passCompatId());
	QCOMPARE(dup->name, spec.newName);
	QCOMPARE(dup->author, spec.newAuthor);
}

void TestLibraryManager::duplicateLibraryRejectsIdCollision() {
	LibraryManager mgr;
	mgr.loadAll();

	LibraryManager::DuplicateSpec spec;
	spec.newId = LibraryManager::myLibraryId();  // 既存 id と衝突
	spec.newName = QStringLiteral("x");
	spec.newAuthor = QStringLiteral("x");
	spec.newVersion = QStringLiteral("1.0");
	spec.newLicense.kind = LicenseKind::CC0_1_0;

	const auto result = mgr.duplicateLibrary(LibraryManager::passCompatId(), spec);
	QVERIFY(!result.ok);
}

void TestLibraryManager::removeLibraryRefusesBuiltins() {
	LibraryManager mgr;
	mgr.loadAll();
	QVERIFY(!mgr.removeLibrary(LibraryManager::myLibraryId()));
	QVERIFY(!mgr.removeLibrary(LibraryManager::passCompatId()));

	LibraryManager::DuplicateSpec spec;
	spec.newId = QStringLiteral("removable");
	spec.newName = QStringLiteral("x");
	spec.newAuthor = QStringLiteral("x");
	spec.newVersion = QStringLiteral("1.0");
	spec.newLicense.kind = LicenseKind::CC0_1_0;
	QVERIFY(mgr.duplicateLibrary(LibraryManager::passCompatId(), spec).ok);
	QVERIFY(mgr.removeLibrary("removable"));
	QVERIFY(mgr.library("removable") == nullptr);
}

void TestLibraryManager::exportLibraryRespectsRedistributionFlag() {
	LibraryManager mgr;
	mgr.loadAll();

	QTemporaryDir dir;
	QVERIFY(dir.isValid());

	// マイライブラリ・PasS互換はエクスポート不可。
	QVERIFY(!mgr.exportLibrary(LibraryManager::myLibraryId(), dir.filePath("a.blib")));
	QVERIFY(!mgr.exportLibrary(LibraryManager::passCompatId(), dir.filePath("b.blib")));

	// CC0 で複製したものはエクスポート可能。
	LibraryManager::DuplicateSpec spec;
	spec.newId = QStringLiteral("exportable-dup");
	spec.newName = QStringLiteral("x");
	spec.newAuthor = QStringLiteral("x");
	spec.newVersion = QStringLiteral("1.0");
	spec.newLicense.kind = LicenseKind::CC0_1_0;
	QVERIFY(mgr.duplicateLibrary(LibraryManager::passCompatId(), spec).ok);
	QVERIFY(mgr.exportLibrary("exportable-dup", dir.filePath("c.blib")));
	QVERIFY(QFileInfo::exists(dir.filePath("c.blib")));
}

void TestLibraryManager::persistedLibrariesSurviveReload() {
	{
		LibraryManager mgr;
		mgr.loadAll();
		LibraryManager::DuplicateSpec spec;
		spec.newId = QStringLiteral("persisted-lib");
		spec.newName = QStringLiteral("永続化テスト");
		spec.newAuthor = QStringLiteral("tomo-x");
		spec.newVersion = QStringLiteral("2.0.0");
		spec.newLicense.kind = LicenseKind::MIT;
		QVERIFY(mgr.duplicateLibrary(LibraryManager::passCompatId(), spec).ok);
	}
	// 新しい LibraryManager インスタンスで読み直し、ディスクに保存されていることを確認する。
	LibraryManager mgr2;
	mgr2.loadAll();
	const auto lib = mgr2.library("persisted-lib");
	QVERIFY(lib != nullptr);
	QCOMPARE(lib->name, QStringLiteral("永続化テスト"));
	QCOMPARE(lib->version, QStringLiteral("2.0.0"));
	QVERIFY(lib->basedOn.has_value());
}

void TestLibraryManager::addPartToMyLibraryInsertsAndPersists() {
	LibraryManager mgr;
	mgr.loadAll();

	const Part part = makeTestPart("Direct-1");
	const auto result = mgr.addPartToMyLibrary(part, QStringLiteral("misc"));
	QVERIFY2(result.ok, qPrintable(result.error));
	QCOMPARE(result.libraryId, LibraryManager::myLibraryId());

	const auto lib = mgr.library(LibraryManager::myLibraryId());
	QVERIFY(lib->parts.contains("Direct-1"));
	// "misc" は存在しないカテゴリなので、未分類に入るはず。
	QCOMPARE(lib->partCategory.value("Direct-1"), LibraryManager::uncategorizedCategoryId());

	// 同じ id を再度渡すと上書き (編集) になる。
	Part edited = part;
	edited.name = QStringLiteral("編集後の名前");
	QVERIFY(mgr.addPartToMyLibrary(edited).ok);
	QCOMPARE(mgr.library(LibraryManager::myLibraryId())->parts.value("Direct-1")->name,
			 QStringLiteral("編集後の名前"));
}

void TestLibraryManager::addBoardToMyLibraryInsertsAndPersists() {
	LibraryManager mgr;
	mgr.loadAll();

	const BoardSpec board = makeTestBoard("Direct-Board-1");
	const auto result = mgr.addBoardToMyLibrary(board);
	QVERIFY2(result.ok, qPrintable(result.error));

	const auto lib = mgr.library(LibraryManager::myLibraryId());
	QVERIFY(lib->boards.contains("Direct-Board-1"));
	QCOMPARE(lib->boards.value("Direct-Board-1")->cols, 4);
}

void TestLibraryManager::uniqueIdHelpersAvoidCollisionsInMyLibrary() {
	LibraryManager mgr;
	mgr.loadAll();
	QVERIFY(mgr.addPartToMyLibrary(makeTestPart("Dup")).ok);
	QVERIFY(mgr.addBoardToMyLibrary(makeTestBoard("DupBoard")).ok);

	QCOMPARE(mgr.uniquePartIdForMyLibrary("Dup"), QStringLiteral("Dup-2"));
	QCOMPARE(mgr.uniquePartIdForMyLibrary("NotTaken"), QStringLiteral("NotTaken"));
	QCOMPARE(mgr.uniqueBoardIdForMyLibrary("DupBoard"), QStringLiteral("DupBoard-2"));
	QCOMPARE(mgr.uniqueBoardIdForMyLibrary("NotTakenBoard"), QStringLiteral("NotTakenBoard"));
}

void TestLibraryManager::updateLibraryMetadataRefusesReadOnlyLibraries() {
	LibraryManager mgr;
	mgr.loadAll();
	Library updated = *mgr.library(LibraryManager::passCompatId());
	updated.name = QStringLiteral("差し替えを試みる");
	QVERIFY(!mgr.updateLibraryMetadata(LibraryManager::passCompatId(), updated));
	QCOMPARE(mgr.library(LibraryManager::passCompatId())->name, QStringLiteral("PasS互換"));
}

void TestLibraryManager::updateLibraryMetadataUpdatesFieldsButKeepsIdAndContents() {
	LibraryManager mgr;
	mgr.loadAll();
	QVERIFY(mgr.addPartToMyLibrary(makeTestPart("KeepMe")).ok);

	Library updated = *mgr.library(LibraryManager::myLibraryId());
	updated.name = QStringLiteral("私のライブラリ (改名)");
	updated.version = QStringLiteral("2.0.0");
	updated.license.kind = LicenseKind::CC_BY_SA_4_0;
	updated.id = QStringLiteral("try-to-hijack-id");  // 無視されるはず
	updated.readOnly = true;                          // 無視されるはず

	QVERIFY(mgr.updateLibraryMetadata(LibraryManager::myLibraryId(), updated));

	const auto lib = mgr.library(LibraryManager::myLibraryId());
	QCOMPARE(lib->id, LibraryManager::myLibraryId());  // id は変わらない
	QVERIFY(!lib->readOnly);                           // readOnly も変わらない
	QCOMPARE(lib->name, QStringLiteral("私のライブラリ (改名)"));
	QCOMPARE(lib->version, QStringLiteral("2.0.0"));
	QCOMPARE(static_cast<int>(lib->license.kind), static_cast<int>(LicenseKind::CC_BY_SA_4_0));
	QVERIFY(lib->parts.contains("KeepMe"));  // 中身は保持される
}

QTEST_MAIN(TestLibraryManager)
#include "test_librarymanager.moc"
