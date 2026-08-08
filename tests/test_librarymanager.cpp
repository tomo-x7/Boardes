#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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
	void installBlibCreatesIndependentLibrary();
	void installBlibRejectsReservedIds();
	void installLibraryIsIdempotentWhenAlreadyInstalled();
	void duplicateLibraryAppliesNewMetadataAndRedistributionRule();
	void duplicateLibraryRejectsIdCollision();
	void removeLibraryAllowsBuiltinsAndRecreatesEmpty();
	void exportLibraryAllowsNonRedistributableForBackup();
	void persistedLibrariesSurviveReload();

	void addPartToMyLibraryInsertsAndPersists();
	void addBoardToMyLibraryInsertsAndPersists();
	void uniqueIdHelpersAvoidCollisionsInMyLibrary();
	void updateLibraryMetadataAllowsAnyLibraryIncludingPassCompat();
	void updateLibraryMetadataUpdatesFieldsButKeepsIdAndContents();

	void createLibraryRejectsDuplicateId();
	void categoryLifecycleAddRenameDeleteMovesPartsToUncategorized();
	void reorderCategoriesAppliesOrderField();
	void addPartToAndRemovePartFromArbitraryLibrary();
	void setPartCategoryValidatesCategoryExists();
	void addBoardToAndRemoveBoardFromArbitraryLibrary();
	void copyPartsBetweenLibrariesRecordsBasedOnWhenSourceNotRedistributable();
	void copyBoardsBetweenLibraries();
	void persistFailureLeavesExistingLibraryIntact();

	void loadAllRecordsIssueForLibraryWithInvalidSchema();
	void loadAllRecordsIssueForLibraryWithBrokenPartReference();
	void loadAllSucceedsWithUnknownPartCategoryReference();

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

	// Phase 14: readOnly は廃止。どちらも既定では再配布不可 (ライセンスがそうなっているため)。
	const auto myLib = mgr.library(LibraryManager::myLibraryId());
	QVERIFY(myLib != nullptr);
	QVERIFY(!myLib->redistribution.allowed);

	const auto passLib = mgr.library(LibraryManager::passCompatId());
	QVERIFY(passLib != nullptr);
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

void TestLibraryManager::installBlibCreatesIndependentLibrary() {
	Library lib;
	lib.id = QStringLiteral("third-party-lib");
	lib.name = QStringLiteral("サードパーティ");
	lib.license.kind = LicenseKind::MIT;
	lib.redistribution = redistributionRuleFor(LicenseKind::MIT);
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
	QVERIFY(installed->parts.contains("TP-1"));
	// Phase 14: インストール後も編集できる。
	QVERIFY(mgr.addPartTo("third-party-lib", makeTestPart("TP-2")).ok);
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
	spec.newLicense.kind = LicenseKind::CC_BY_4_0;

	const auto result = mgr.duplicateLibrary(LibraryManager::passCompatId(), spec);
	QVERIFY2(result.ok, qPrintable(result.error));

	const auto dup = mgr.library("my-pass-fork");
	QVERIFY(dup != nullptr);
	QVERIFY(dup->redistribution.allowed);
	QVERIFY(dup->redistribution.attributionRequired);
	QCOMPARE(dup->basedOn.size(), 1);
	QCOMPARE(dup->basedOn[0].libraryId, LibraryManager::passCompatId());
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

void TestLibraryManager::removeLibraryAllowsBuiltinsAndRecreatesEmpty() {
	// Phase 14: readOnly 廃止に伴い、マイライブラリ・PasS互換も削除できる。ただし
	// 他のコードが「常に存在する」ことを前提にしているため、削除直後に空の状態で
	// 作り直される (壊れた状態のまま残らないようにするため)。
	LibraryManager mgr;
	mgr.loadAll();
	QVERIFY(mgr.addPartToMyLibrary(makeTestPart("WillBeGone")).ok);

	QVERIFY(mgr.removeLibrary(LibraryManager::myLibraryId()));
	const auto recreated = mgr.library(LibraryManager::myLibraryId());
	QVERIFY(recreated != nullptr);
	QVERIFY(!recreated->parts.contains("WillBeGone"));  // 中身は空で作り直される

	QVERIFY(mgr.removeLibrary(LibraryManager::passCompatId()));
	QVERIFY(mgr.library(LibraryManager::passCompatId()) != nullptr);

	QVERIFY(!mgr.removeLibrary(QStringLiteral("no-such-library")));
}

void TestLibraryManager::exportLibraryAllowsNonRedistributableForBackup() {
	// Phase 14: 「手元のファイルはユーザーのもの」方針により、再配布不可のライブラリでも
	// バックアップ用途でエクスポートできる (警告は呼び出し側 UI の責務)。
	LibraryManager mgr;
	mgr.loadAll();

	QTemporaryDir dir;
	QVERIFY(dir.isValid());

	QVERIFY(mgr.exportLibrary(LibraryManager::myLibraryId(), dir.filePath("a.blib")));
	QVERIFY(QFileInfo::exists(dir.filePath("a.blib")));
	QVERIFY(mgr.exportLibrary(LibraryManager::passCompatId(), dir.filePath("b.blib")));
	QVERIFY(QFileInfo::exists(dir.filePath("b.blib")));

	// 存在しない id は失敗する。
	QVERIFY(!mgr.exportLibrary(QStringLiteral("no-such-library"), dir.filePath("c.blib")));
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
	QCOMPARE(lib->basedOn.size(), 1);
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

	// 汎用版 (任意のライブラリ id を取る) も同じ結果になる。
	QCOMPARE(mgr.uniquePartId(LibraryManager::myLibraryId(), "Dup"), QStringLiteral("Dup-2"));
	QCOMPARE(mgr.uniqueBoardId(LibraryManager::myLibraryId(), "DupBoard"), QStringLiteral("DupBoard-2"));
}

void TestLibraryManager::updateLibraryMetadataAllowsAnyLibraryIncludingPassCompat() {
	// Phase 14: readOnly 廃止により、PasS互換も含めてどのライブラリでもメタデータ編集できる。
	LibraryManager mgr;
	mgr.loadAll();
	Library updated = *mgr.library(LibraryManager::passCompatId());
	updated.name = QStringLiteral("PasS互換 (編集後)");
	QVERIFY(mgr.updateLibraryMetadata(LibraryManager::passCompatId(), updated));
	QCOMPARE(mgr.library(LibraryManager::passCompatId())->name, QStringLiteral("PasS互換 (編集後)"));
}

void TestLibraryManager::updateLibraryMetadataUpdatesFieldsButKeepsIdAndContents() {
	LibraryManager mgr;
	mgr.loadAll();
	QVERIFY(mgr.addPartToMyLibrary(makeTestPart("KeepMe")).ok);

	Library updated = *mgr.library(LibraryManager::myLibraryId());
	updated.name = QStringLiteral("私のライブラリ (改名)");
	updated.version = QStringLiteral("2.0.0");
	updated.license.kind = LicenseKind::CC_BY_4_0;
	updated.id = QStringLiteral("try-to-hijack-id");  // 無視されるはず

	QVERIFY(mgr.updateLibraryMetadata(LibraryManager::myLibraryId(), updated));

	const auto lib = mgr.library(LibraryManager::myLibraryId());
	QCOMPARE(lib->id, LibraryManager::myLibraryId());  // id は変わらない
	QCOMPARE(lib->name, QStringLiteral("私のライブラリ (改名)"));
	QCOMPARE(lib->version, QStringLiteral("2.0.0"));
	QCOMPARE(static_cast<int>(lib->license.kind), static_cast<int>(LicenseKind::CC_BY_4_0));
	QVERIFY(lib->parts.contains("KeepMe"));  // 中身は保持される
}

void TestLibraryManager::createLibraryRejectsDuplicateId() {
	LibraryManager mgr;
	mgr.loadAll();

	Library meta;
	meta.id = QStringLiteral("brand-new-lib");
	meta.name = QStringLiteral("新規ライブラリ");
	const auto result = mgr.createLibrary(meta);
	QVERIFY2(result.ok, qPrintable(result.error));
	QVERIFY(mgr.library("brand-new-lib") != nullptr);
	// 未分類カテゴリが自動で用意される。
	QVERIFY(mgr.library("brand-new-lib")->category(LibraryManager::uncategorizedCategoryId()) != nullptr);

	const auto dupResult = mgr.createLibrary(meta);
	QVERIFY(!dupResult.ok);

	Library noId;
	QVERIFY(!mgr.createLibrary(noId).ok);
}

void TestLibraryManager::categoryLifecycleAddRenameDeleteMovesPartsToUncategorized() {
	LibraryManager mgr;
	mgr.loadAll();
	const QString libId = LibraryManager::myLibraryId();

	CategoryInfo cat;
	cat.id = QStringLiteral("resistors");
	cat.name = QStringLiteral("抵抗");
	QVERIFY(mgr.addCategory(libId, cat));
	QVERIFY(!mgr.addCategory(libId, cat));  // 重複は失敗

	QVERIFY(mgr.addPartTo(libId, makeTestPart("R-1"), QStringLiteral("resistors")).ok);
	QCOMPARE(mgr.library(libId)->partCategory.value("R-1"), QStringLiteral("resistors"));

	CategoryInfo renamed = cat;
	renamed.name = QStringLiteral("抵抗器");
	QVERIFY(mgr.updateCategory(libId, renamed));
	QCOMPARE(mgr.library(libId)->category("resistors")->name, QStringLiteral("抵抗器"));

	QVERIFY(!mgr.removeCategory(libId, LibraryManager::uncategorizedCategoryId()));  // 未分類は削除不可
	QVERIFY(mgr.removeCategory(libId, QStringLiteral("resistors")));
	QVERIFY(mgr.library(libId)->category("resistors") == nullptr);
	// 中の部品は未分類へ移動する。
	QCOMPARE(mgr.library(libId)->partCategory.value("R-1"), LibraryManager::uncategorizedCategoryId());
}

void TestLibraryManager::reorderCategoriesAppliesOrderField() {
	LibraryManager mgr;
	mgr.loadAll();
	const QString libId = LibraryManager::myLibraryId();

	CategoryInfo a, b;
	a.id = "a";
	a.name = "A";
	b.id = "b";
	b.name = "B";
	QVERIFY(mgr.addCategory(libId, a));
	QVERIFY(mgr.addCategory(libId, b));

	QVERIFY(mgr.reorderCategories(libId, {"b", "a", LibraryManager::uncategorizedCategoryId()}));
	QCOMPARE(mgr.library(libId)->category("b")->order, 0);
	QCOMPARE(mgr.library(libId)->category("a")->order, 1);
}

void TestLibraryManager::addPartToAndRemovePartFromArbitraryLibrary() {
	LibraryManager mgr;
	mgr.loadAll();
	Library other;
	other.id = QStringLiteral("other-lib");
	other.name = QStringLiteral("他のライブラリ");
	QVERIFY(mgr.createLibrary(other).ok);

	QVERIFY(mgr.addPartTo("other-lib", makeTestPart("P1")).ok);
	QVERIFY(mgr.library("other-lib")->parts.contains("P1"));

	QVERIFY(!mgr.removePartFrom("other-lib", "no-such-part"));
	QVERIFY(mgr.removePartFrom("other-lib", "P1"));
	QVERIFY(!mgr.library("other-lib")->parts.contains("P1"));
}

void TestLibraryManager::setPartCategoryValidatesCategoryExists() {
	LibraryManager mgr;
	mgr.loadAll();
	const QString libId = LibraryManager::myLibraryId();
	QVERIFY(mgr.addPartTo(libId, makeTestPart("P1")).ok);

	QVERIFY(!mgr.setPartCategory(libId, "P1", QStringLiteral("no-such-category")));
	CategoryInfo cat;
	cat.id = "diodes";
	cat.name = "ダイオード";
	QVERIFY(mgr.addCategory(libId, cat));
	QVERIFY(mgr.setPartCategory(libId, "P1", "diodes"));
	QCOMPARE(mgr.library(libId)->partCategory.value("P1"), QStringLiteral("diodes"));
}

void TestLibraryManager::addBoardToAndRemoveBoardFromArbitraryLibrary() {
	LibraryManager mgr;
	mgr.loadAll();
	QVERIFY(mgr.addBoardTo(LibraryManager::passCompatId(), makeTestBoard("B1")).ok);
	QVERIFY(mgr.library(LibraryManager::passCompatId())->boards.contains("B1"));
	QVERIFY(mgr.removeBoardFrom(LibraryManager::passCompatId(), "B1"));
	QVERIFY(!mgr.library(LibraryManager::passCompatId())->boards.contains("B1"));
}

void TestLibraryManager::copyPartsBetweenLibrariesRecordsBasedOnWhenSourceNotRedistributable() {
	LibraryManager mgr;
	mgr.loadAll();
	QVERIFY(mgr.addPartTo(LibraryManager::passCompatId(), makeTestPart("Src-1")).ok);  // 再配布不可ライブラリ
	Library other;
	other.id = "copy-dest";
	other.name = "コピー先";
	QVERIFY(mgr.createLibrary(other).ok);

	const auto result = mgr.copyPartsBetween(LibraryManager::passCompatId(), {"Src-1"}, "copy-dest", QString());
	QVERIFY2(result.ok, qPrintable(result.error));
	QCOMPARE(result.partCount, 1);
	QVERIFY(mgr.library("copy-dest")->parts.contains("Src-1"));
	QCOMPARE(mgr.library("copy-dest")->basedOn.size(), 1);
	QCOMPARE(mgr.library("copy-dest")->basedOn[0].libraryId, LibraryManager::passCompatId());

	// id 衝突は自動リネームされる (同じ部品をもう一度複製)。
	const auto result2 = mgr.copyPartsBetween(LibraryManager::passCompatId(), {"Src-1"}, "copy-dest", QString());
	QVERIFY(result2.ok);
	QVERIFY(mgr.library("copy-dest")->parts.contains("Src-1-2"));
}

void TestLibraryManager::copyBoardsBetweenLibraries() {
	LibraryManager mgr;
	mgr.loadAll();
	QVERIFY(mgr.addBoardTo(LibraryManager::myLibraryId(), makeTestBoard("SrcBoard")).ok);
	Library other;
	other.id = "board-copy-dest";
	other.name = "基板コピー先";
	QVERIFY(mgr.createLibrary(other).ok);

	const auto result = mgr.copyBoardsBetween(LibraryManager::myLibraryId(), {"SrcBoard"}, "board-copy-dest");
	QVERIFY2(result.ok, qPrintable(result.error));
	QVERIFY(mgr.library("board-copy-dest")->boards.contains("SrcBoard"));
}

void TestLibraryManager::persistFailureLeavesExistingLibraryIntact() {
	// persist() は一時ディレクトリに書き切ってから入れ替える (Phase 14)。書き込み中に
	// 失敗しても既存のライブラリが消えないことを確認する。ここでは
	// AppDataLocation/libraries/ を読み取り専用にして、"<id>.tmp" の作成自体を失敗させる。
	LibraryManager mgr;
	mgr.loadAll();
	QVERIFY(mgr.addPartToMyLibrary(makeTestPart("Survivor")).ok);

	QVERIFY(QFile::setPermissions(mgr.storageDir().toUtf8(), QFileDevice::ReadOwner | QFileDevice::ExeOwner));
	// root で実行しているなど、パーミッションが効かない環境では前提が崩れるのでスキップする。
	const QString probePath = mgr.storageDir() + QStringLiteral("/.write-probe");
	QFile probe(probePath);
	const bool writable = probe.open(QIODevice::WriteOnly);
	if (writable) {
		probe.close();
		QFile::remove(probePath);
	}
	if (!writable) {
		const auto result = mgr.addPartToMyLibrary(makeTestPart("ShouldFail"));
		QVERIFY(!result.ok);
		QVERIFY(mgr.library(LibraryManager::myLibraryId())->parts.contains("Survivor"));
	}

	// 権限を戻す (後片付け。QTemporaryDir の削除に必要)。
	QFile::setPermissions(mgr.storageDir().toUtf8(),
						  QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);

	if (!writable) {
		LibraryManager mgr2;
		mgr2.loadAll();
		QVERIFY(mgr2.library(LibraryManager::myLibraryId())->parts.contains("Survivor"));
	} else {
		QSKIP("この環境ではディレクトリのパーミッションで書き込みを禁止できないため検証をスキップします");
	}
}

void TestLibraryManager::loadAllRecordsIssueForLibraryWithInvalidSchema() {
	LibraryManager mgr;

	// storageDir() 直下に、schema が未来のバージョンを指す library.json を直接置く。
	const QString brokenDir = mgr.storageDir() + QStringLiteral("/broken-schema-lib");
	QVERIFY(QDir().mkpath(brokenDir));
	QJsonObject obj;
	obj["schema"] = QStringLiteral("boardes.library/99");
	obj["id"] = QStringLiteral("broken-schema-lib");
	obj["name"] = QStringLiteral("壊れたライブラリ");
	QFile f(brokenDir + QStringLiteral("/library.json"));
	QVERIFY(f.open(QIODevice::WriteOnly));
	f.write(QJsonDocument(obj).toJson());
	f.close();

	mgr.loadAll();
	QVERIFY(mgr.library(QStringLiteral("broken-schema-lib")) == nullptr);
	// 他のライブラリの破損に引きずられず、マイライブラリ・PasS互換は通常どおり作られる。
	QVERIFY(mgr.library(LibraryManager::myLibraryId()) != nullptr);
	QVERIFY(mgr.library(LibraryManager::passCompatId()) != nullptr);

	const auto issues = mgr.loadIssues();
	QCOMPARE(issues.size(), 1);
	QCOMPARE(issues[0].directory, brokenDir);
	QVERIFY(!issues[0].reason.isEmpty());
}

void TestLibraryManager::loadAllRecordsIssueForLibraryWithBrokenPartReference() {
	LibraryManager mgr;

	// library.json は正しい形式だが、参照している .part.json が存在しない。
	const QString libDir = mgr.storageDir() + QStringLiteral("/broken-part-ref-lib");
	QVERIFY(QDir().mkpath(libDir));
	QJsonObject obj;
	obj["schema"] = QStringLiteral("boardes.library/1");
	obj["id"] = QStringLiteral("broken-part-ref-lib");
	obj["name"] = QStringLiteral("部品参照が壊れたライブラリ");
	QJsonObject license;
	license["kind"] = QStringLiteral("all-rights-reserved");
	obj["license"] = license;
	QJsonArray parts;
	QJsonObject p;
	p["id"] = QStringLiteral("GHOST-1");
	p["file"] = QStringLiteral("parts/GHOST-1.part.json");  // 実在しない
	parts.append(p);
	obj["parts"] = parts;
	QFile f(libDir + QStringLiteral("/library.json"));
	QVERIFY(f.open(QIODevice::WriteOnly));
	f.write(QJsonDocument(obj).toJson());
	f.close();

	mgr.loadAll();
	QVERIFY(mgr.library(QStringLiteral("broken-part-ref-lib")) == nullptr);

	const auto issues = mgr.loadIssues();
	QCOMPARE(issues.size(), 1);
	QCOMPARE(issues[0].directory, libDir);
	QVERIFY2(issues[0].reason.contains(QStringLiteral("GHOST-1")), qPrintable(issues[0].reason));
}

void TestLibraryManager::loadAllSucceedsWithUnknownPartCategoryReference() {
	// 存在しないカテゴリを参照している部品は、致命的ではなく未分類に倒されて
	// ライブラリ全体としては正常に読み込めるべき (Phase 17)。
	Library lib;
	lib.id = QStringLiteral("stray-category-lib");
	lib.name = QStringLiteral("カテゴリ参照が浮いたライブラリ");
	lib.license.kind = LicenseKind::AllRightsReserved;
	auto part = std::make_shared<Part>(makeTestPart("Stray-1"));
	lib.parts.insert(part->id, part);
	lib.partCategory.insert(part->id, QStringLiteral("no-such-category"));  // カテゴリ自体は未定義

	LibraryManager mgr;
	const QString libDir = mgr.storageDir() + QStringLiteral("/stray-category-lib");
	QVERIFY(libraryio::saveToDirectory(lib, libDir));

	mgr.loadAll();
	const auto loaded = mgr.library(QStringLiteral("stray-category-lib"));
	QVERIFY(loaded != nullptr);
	QVERIFY(loaded->parts.contains(QStringLiteral("Stray-1")));
	// 未分類に倒されていること。
	QCOMPARE(loaded->partCategory.value(QStringLiteral("Stray-1")), QString());
	QVERIFY(mgr.loadIssues().isEmpty());
}

QTEST_MAIN(TestLibraryManager)
#include "test_librarymanager.moc"
