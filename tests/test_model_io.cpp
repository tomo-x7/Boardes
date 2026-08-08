#include <QBuffer>
#include <QDir>
#include <QImage>
#include <QJsonArray>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "io/boardio.h"
#include "io/documentio.h"
#include "io/jsonutil.h"
#include "io/libraryio.h"
#include "io/partio.h"
#include "io/zipio.h"
#include "model/document.h"
#include "model/library.h"

namespace {

QImage makeTestImage() {
	// 4x2 の小さなテスト画像。(0,0) は後でクロマキー色にする。
	QImage img(4, 2, QImage::Format_RGB888);
	img.fill(Qt::blue);
	img.setPixelColor(0, 0, QColor(187, 201, 158));
	img.setPixelColor(3, 1, QColor(255, 128, 64));
	return img;
}

Part makeTestPart() {
	Part part;
	part.id = QStringLiteral("R-2");
	part.name = QStringLiteral("テスト抵抗");
	part.description = QStringLiteral("説明文");
	part.keywords = {QStringLiteral("resistor"), QStringLiteral("抵抗")};
	part.kind = PartKind::Normal;
	part.refPrefix = QStringLiteral("R");
	part.outline = QRect(0, 0, 4, 2);
	part.artwork = Artwork::fromChromaKeyed(makeTestImage(), QColor(187, 201, 158));
	part.pins = {
		Pin{1, QPoint(0, 0), 0, QString()},
		Pin{2, QPoint(3, 1), 9, QString()},
	};
	// 明示的な基準点 (anchor) 付き。Phase 11 で追加したフィールドのラウンドトリップを
	// 他のテスト (embedded/sidecar/document/library) すべてに乗せて検証する。
	part.anchor = QPoint(3, 1);
	part.anchorExplicit = true;
	return part;
}

bool imagesEqual(QImage a, QImage b) {
	return a.convertToFormat(QImage::Format_ARGB32) == b.convertToFormat(QImage::Format_ARGB32);
}

bool partsEqual(const Part &a, const Part &b) {
	if (a.id != b.id || a.name != b.name || a.description != b.description) return false;
	if (a.keywords != b.keywords) return false;
	if (a.kind != b.kind || a.refPrefix != b.refPrefix) return false;
	if (a.outline != b.outline) return false;
	if (a.anchor != b.anchor || a.anchorExplicit != b.anchorExplicit) return false;
	if (a.pins.size() != b.pins.size()) return false;
	for (int i = 0; i < a.pins.size(); ++i) {
		if (a.pins[i].number != b.pins[i].number) return false;
		if (a.pins[i].pos != b.pins[i].pos) return false;
		if (a.pins[i].drill != b.pins[i].drill) return false;
	}
	return imagesEqual(a.artwork.image, b.artwork.image);
}

BoardSpec makeTestBoard() {
	BoardSpec board;
	board.id = QStringLiteral("TESTB");
	board.name = QStringLiteral("テスト基板");
	board.size = QSize(100, 80);
	board.cols = 8;
	board.rows = 6;
	board.pitch = 10;
	board.origin = QPoint(10, 10);
	board.absentHoles = {QPoint(0, 0)};
	board.padShape = PadShape::Round;
	board.padDiameter = 6;
	board.holeDiameter = 3;
	board.copper = CopperPattern::PadPerHole;
	board.doubleSided = true;
	// 既定値と異なる色を明示的に設定する。boardio.cpp の色コーデック (これまで未検証だった)
	// と、既定値 3箇所 (board.h / boardio.cpp / boardeditordialog.h) の食い違いを防ぐ。
	board.substrateColor = QColor(10, 20, 30);
	board.padColor = QColor(40, 50, 60);
	board.copperColor = QColor(70, 80, 90);
	board.mountingHoles = {{QPoint(5, 5), 30}};
	board.backgroundFront = Artwork::fromImageAsIs(makeTestImage());
	board.outlineRect = QRect(0, 0, 100, 80);
	return board;
}

// Phase 17: バリデーションテスト用に、部品1個・配線1本を持つ最小限の正常なドキュメント
// JSON を作る。各テストはこれを起点にフィールドを1箇所だけ壊して検証する。
QJsonObject makeValidDocumentJson() {
	Document doc;
	doc.title = QStringLiteral("バリデーションテスト");
	doc.board = makeTestBoard();

	Placement placement;
	placement.uuid = QStringLiteral("11111111-1111-1111-1111-111111111111");
	placement.libraryId = QStringLiteral("pass-compat");
	placement.partId = QStringLiteral("R-2");
	placement.pos = QPoint(0, 0);
	placement.rot = Rotation::R0;
	placement.side = Side::Front;
	doc.placements.append(std::make_shared<Placement>(placement));

	Wire wire;
	wire.uuid = QStringLiteral("22222222-2222-2222-2222-222222222222");
	wire.layer = WireLayer::FrontBare;
	wire.points = {QPoint(0, 0), QPoint(10, 0)};
	doc.wires.append(std::make_shared<Wire>(wire));

	return documentio::toJsonObject(doc);
}

}  // namespace

class TestModelIo : public QObject {
	Q_OBJECT

private slots:
	void partEmbeddedRoundTrip();
	void partSidecarRoundTrip();
	void partEncodingsAreEquivalent();
	void partValidationRejectsUnknownArtworkEncoding();
	void partValidationRejectsMissingSidecarImage();
	void boardRoundTrip();
	void boardValidationRejectsInvalidColor();
	void boardValidationRejectsBrokenBackgroundBase64();
	void documentRoundTrip();
	void documentValidationRejectsTruncatedJson();
	void documentValidationRejectsInvalidRotation();
	void documentValidationRejectsDuplicatePlacementUuid();
	void documentValidationRejectsShortWire();
	void documentValidationRejectsFutureSchemaVersion();
	void documentValidationRejectsNonObjectBoard();
	void libraryDirectoryRoundTrip();
	void libraryBlibRoundTrip();
	void zipRoundTripWithUnicodeName();
	void documentPackageExportSkipsNonRedistributable();
};

void TestModelIo::partEmbeddedRoundTrip() {
	const Part original = makeTestPart();
	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	const QString path = dir.filePath("test.bpart");
	QVERIFY(partio::saveEmbedded(original, path));

	const auto loaded = partio::loadEmbedded(path);
	QVERIFY(loaded.has_value());
	QVERIFY(partsEqual(original, *loaded));
}

void TestModelIo::partSidecarRoundTrip() {
	const Part original = makeTestPart();
	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	const QString jsonPath = dir.filePath("R-2.part.json");
	QVERIFY(partio::saveSidecar(original, jsonPath));
	QVERIFY(QFileInfo::exists(dir.filePath("R-2.png")));

	const auto loaded = partio::loadSidecar(jsonPath);
	QVERIFY(loaded.has_value());
	QVERIFY(partsEqual(original, *loaded));
}

void TestModelIo::partEncodingsAreEquivalent() {
	const Part original = makeTestPart();
	QTemporaryDir dir;
	QVERIFY(dir.isValid());

	QVERIFY(partio::saveEmbedded(original, dir.filePath("a.bpart")));
	QVERIFY(partio::saveSidecar(original, dir.filePath("a.part.json")));

	const auto embedded = partio::loadEmbedded(dir.filePath("a.bpart"));
	const auto sidecar = partio::loadSidecar(dir.filePath("a.part.json"));
	QVERIFY(embedded.has_value());
	QVERIFY(sidecar.has_value());
	QVERIFY(partsEqual(*embedded, *sidecar));
}

void TestModelIo::partValidationRejectsUnknownArtworkEncoding() {
	const Part original = makeTestPart();
	QJsonObject obj = partio::toJsonObject(original, /*embedBase64=*/true);
	QJsonObject artwork = obj["artwork"].toObject();
	artwork["encoding"] = QStringLiteral("weird");
	obj["artwork"] = artwork;

	const LoadResult result = partio::validateJson(obj, QStringLiteral("part"));
	QVERIFY(!result.ok);
	QVERIFY2(result.detail.contains(QStringLiteral("encoding")), qPrintable(result.detail));
}

void TestModelIo::partValidationRejectsMissingSidecarImage() {
	const Part original = makeTestPart();
	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	const QString jsonPath = dir.filePath("R-2.part.json");
	QVERIFY(partio::saveSidecar(original, jsonPath));
	// サイドカー画像だけを消す (part.json 自体は正しい形式のまま)。
	QVERIFY(QFile::remove(dir.filePath("R-2.png")));

	LoadResult result;
	const auto loaded = partio::loadSidecar(jsonPath, &result);
	QVERIFY(!loaded.has_value());
	QVERIFY(!result.ok);
}

void TestModelIo::boardRoundTrip() {
	const BoardSpec original = makeTestBoard();
	const QJsonObject obj = boardio::toJsonObject(original);
	const BoardSpec restored = boardio::fromJsonObject(obj);

	QCOMPARE(restored.id, original.id);
	QCOMPARE(restored.name, original.name);
	QCOMPARE(restored.size, original.size);
	QCOMPARE(restored.cols, original.cols);
	QCOMPARE(restored.rows, original.rows);
	QCOMPARE(restored.pitch, original.pitch);
	QCOMPARE(restored.origin, original.origin);
	QCOMPARE(restored.absentHoles, original.absentHoles);
	QCOMPARE(static_cast<int>(restored.padShape), static_cast<int>(original.padShape));
	QCOMPARE(static_cast<int>(restored.copper), static_cast<int>(original.copper));
	QCOMPARE(restored.doubleSided, original.doubleSided);
	QCOMPARE(restored.substrateColor, original.substrateColor);
	QCOMPARE(restored.padColor, original.padColor);
	QCOMPARE(restored.copperColor, original.copperColor);
	QCOMPARE(restored.mountingHoles.size(), original.mountingHoles.size());
	QVERIFY(restored.backgroundFront.has_value());
	QVERIFY(imagesEqual(restored.backgroundFront->image, original.backgroundFront->image));
	QCOMPARE(restored.outlineRect, original.outlineRect);
}

void TestModelIo::boardValidationRejectsInvalidColor() {
	const BoardSpec board = makeTestBoard();
	QJsonObject obj = boardio::toJsonObject(board);
	QJsonObject colors = obj["colors"].toObject();
	colors["substrate"] = QStringLiteral("not-a-color");
	obj["colors"] = colors;

	const LoadResult result = boardio::validateJson(obj, QStringLiteral("board"));
	QVERIFY(!result.ok);
	QVERIFY2(result.detail.contains(QStringLiteral("colors")), qPrintable(result.detail));
}

void TestModelIo::boardValidationRejectsBrokenBackgroundBase64() {
	const BoardSpec board = makeTestBoard();
	QJsonObject obj = boardio::toJsonObject(board);
	QJsonObject bg = obj["background"].toObject();
	QJsonObject front = bg["front"].toObject();
	front["data"] = QStringLiteral("!!!not valid base64!!!");
	bg["front"] = front;
	obj["background"] = bg;

	const LoadResult result = boardio::validateJson(obj, QStringLiteral("board"));
	QVERIFY(!result.ok);
	QVERIFY2(result.detail.contains(QStringLiteral("background")), qPrintable(result.detail));
}

void TestModelIo::documentRoundTrip() {
	Document doc;
	doc.title = QStringLiteral("テスト設計");
	doc.author = QStringLiteral("tomo-x");
	doc.notes = QStringLiteral("メモ");
	doc.board = makeTestBoard();
	doc.dependencies.append(Dependency{QStringLiteral("pass-compat"), QStringLiteral("PasS互換"), QStringLiteral("0"),
									   QString(), false});
	Placement placement;
	placement.uuid = QStringLiteral("11111111-1111-1111-1111-111111111111");
	placement.libraryId = QStringLiteral("pass-compat");
	placement.partId = QStringLiteral("R-2");
	placement.pos = QPoint(20, 30);
	placement.rot = Rotation::R90;
	placement.side = Side::Back;
	placement.refDes = QStringLiteral("R1");
	placement.value = QStringLiteral("10k");
	doc.placements.append(std::make_shared<Placement>(placement));

	Wire wire;
	wire.uuid = QStringLiteral("22222222-2222-2222-2222-222222222222");
	wire.layer = WireLayer::BackBare;
	wire.points = {QPoint(0, 0), QPoint(10, 0), QPoint(10, 10)};
	doc.wires.append(std::make_shared<Wire>(wire));

	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	const QString path = dir.filePath("test.boardes");
	QVERIFY(documentio::save(doc, path));

	Document loaded;
	QVERIFY(documentio::load(path, loaded));
	QCOMPARE(loaded.title, doc.title);
	QCOMPARE(loaded.author, doc.author);
	QCOMPARE(loaded.notes, doc.notes);
	QCOMPARE(loaded.board.id, doc.board.id);
	QCOMPARE(loaded.dependencies.size(), 1);
	QCOMPARE(loaded.dependencies[0].libraryId, QStringLiteral("pass-compat"));
	QCOMPARE(loaded.placements.size(), 1);
	QCOMPARE(loaded.placements[0]->refDes, QStringLiteral("R1"));
	QCOMPARE(static_cast<int>(loaded.placements[0]->rot), static_cast<int>(Rotation::R90));
	QCOMPARE(static_cast<int>(loaded.placements[0]->side), static_cast<int>(Side::Back));
	QCOMPARE(loaded.wires.size(), 1);
	QCOMPARE(loaded.wires[0]->points.size(), 3);
	QCOMPARE(static_cast<int>(loaded.wires[0]->layer), static_cast<int>(WireLayer::BackBare));
}

void TestModelIo::documentValidationRejectsTruncatedJson() {
	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	const QString path = dir.filePath("broken.boardes");
	QFile f(path);
	QVERIFY(f.open(QIODevice::WriteOnly));
	// 意図的に途中で切れた JSON。
	f.write("{ \"schema\": \"boardes.document/1\", \"board\": {");
	f.close();

	Document doc;
	const LoadResult result = documentio::load(path, doc);
	QVERIFY(!result.ok);
	QVERIFY(!result.detail.isEmpty());
}

void TestModelIo::documentValidationRejectsInvalidRotation() {
	QJsonObject obj = makeValidDocumentJson();
	QJsonArray placements = obj["placements"].toArray();
	QJsonObject p0 = placements[0].toObject();
	p0["rot"] = 45;
	placements[0] = p0;
	obj["placements"] = placements;

	Document loaded;
	const LoadResult result = documentio::validateAndLoad(obj, loaded);
	QVERIFY(!result.ok);
	QVERIFY2(result.detail.contains(QStringLiteral("rot")), qPrintable(result.detail));
}

void TestModelIo::documentValidationRejectsDuplicatePlacementUuid() {
	QJsonObject obj = makeValidDocumentJson();
	QJsonArray placements = obj["placements"].toArray();
	QJsonObject dup = placements[0].toObject();
	placements.append(dup);  // 同じ uuid をもう1件追加する
	obj["placements"] = placements;

	Document loaded;
	const LoadResult result = documentio::validateAndLoad(obj, loaded);
	QVERIFY(!result.ok);
	QVERIFY2(result.detail.contains(QStringLiteral("重複")), qPrintable(result.detail));
}

void TestModelIo::documentValidationRejectsShortWire() {
	QJsonObject obj = makeValidDocumentJson();
	QJsonArray wires = obj["wires"].toArray();
	QJsonObject w0 = wires[0].toObject();
	QJsonArray onePoint;
	onePoint.append(jsonutil::fromPoint(QPoint(0, 0)));
	w0["points"] = onePoint;
	wires[0] = w0;
	obj["wires"] = wires;

	Document loaded;
	const LoadResult result = documentio::validateAndLoad(obj, loaded);
	QVERIFY(!result.ok);
	QVERIFY2(result.detail.contains(QStringLiteral("points")), qPrintable(result.detail));
}

void TestModelIo::documentValidationRejectsFutureSchemaVersion() {
	QJsonObject obj = makeValidDocumentJson();
	obj["schema"] = QStringLiteral("boardes.document/99");

	Document loaded;
	const LoadResult result = documentio::validateAndLoad(obj, loaded);
	QVERIFY(!result.ok);
	QVERIFY2(result.summary.contains(QStringLiteral("新しい")), qPrintable(result.summary));
}

void TestModelIo::documentValidationRejectsNonObjectBoard() {
	QJsonObject obj = makeValidDocumentJson();
	obj["board"] = QJsonArray();

	Document loaded;
	const LoadResult result = documentio::validateAndLoad(obj, loaded);
	QVERIFY(!result.ok);
	QVERIFY2(result.detail.contains(QStringLiteral("board")), qPrintable(result.detail));
}

void TestModelIo::libraryDirectoryRoundTrip() {
	Library lib;
	lib.id = QStringLiteral("my-library");
	lib.name = QStringLiteral("マイライブラリ");
	lib.version = QStringLiteral("1.0.0");
	lib.author = QStringLiteral("tomo-x");
	lib.license.kind = LicenseKind::CC_BY_4_0;
	lib.redistribution = redistributionRuleFor(lib.license.kind);
	BasedOn based;
	based.libraryId = QStringLiteral("upstream-lib");
	based.name = QStringLiteral("上流ライブラリ");
	based.version = QStringLiteral("0.9.0");
	based.licenseLabel = QStringLiteral("MIT License");
	lib.basedOn.append(based);

	CategoryInfo cat;
	cat.id = QStringLiteral("R");
	cat.name = QStringLiteral("抵抗");
	cat.icon = makeTestImage();
	lib.categories.append(cat);

	auto part = std::make_shared<Part>(makeTestPart());
	lib.parts.insert(part->id, part);
	lib.partCategory.insert(part->id, cat.id);

	auto board = std::make_shared<BoardSpec>(makeTestBoard());
	lib.boards.insert(board->id, board);

	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	const QString libDir = dir.filePath("mylib");
	QVERIFY(libraryio::saveToDirectory(lib, libDir));

	const auto loaded = libraryio::loadFromDirectory(libDir);
	QVERIFY(loaded.has_value());
	QCOMPARE(loaded->id, lib.id);
	QCOMPARE(loaded->name, lib.name);
	QCOMPARE(static_cast<int>(loaded->license.kind), static_cast<int>(lib.license.kind));
	QCOMPARE(loaded->redistribution.allowed, lib.redistribution.allowed);
	QCOMPARE(loaded->redistribution.attributionRequired, lib.redistribution.attributionRequired);
	QCOMPARE(loaded->basedOn.size(), 1);
	QCOMPARE(loaded->basedOn[0].libraryId, based.libraryId);
	QCOMPARE(loaded->categories.size(), 1);
	QVERIFY(!loaded->categories[0].icon.isNull());
	QCOMPARE(loaded->parts.size(), 1);
	QVERIFY(loaded->parts.contains("R-2"));
	QVERIFY(partsEqual(*loaded->parts["R-2"], *part));
	QCOMPARE(loaded->partCategory.value("R-2"), QStringLiteral("R"));
	QCOMPARE(loaded->boards.size(), 1);
	QVERIFY(loaded->boards.contains("TESTB"));
}

void TestModelIo::libraryBlibRoundTrip() {
	Library lib;
	lib.id = QStringLiteral("exportable-lib");
	lib.name = QStringLiteral("配布用ライブラリ");
	lib.version = QStringLiteral("2.0.0");
	lib.license.kind = LicenseKind::CC0_1_0;
	lib.redistribution = redistributionRuleFor(lib.license.kind);

	auto part = std::make_shared<Part>(makeTestPart());
	lib.parts.insert(part->id, part);
	lib.partCategory.insert(part->id, QStringLiteral("R"));

	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	const QString blibPath = dir.filePath("lib.blib");
	QVERIFY(libraryio::exportToBlib(lib, blibPath));
	QVERIFY(QFileInfo::exists(blibPath));

	const auto loaded = libraryio::importFromBlib(blibPath);
	QVERIFY(loaded.has_value());
	QCOMPARE(loaded->id, lib.id);
	QCOMPARE(loaded->parts.size(), 1);
	QVERIFY(partsEqual(*loaded->parts["R-2"], *part));
}

void TestModelIo::zipRoundTripWithUnicodeName() {
	ZipWriter writer;
	const QByteArray payload1 = "hello boardes";
	const QByteArray payload2 = QStringLiteral("こんにちは").toUtf8();
	QVERIFY(writer.addFile("ascii.txt", payload1));
	QVERIFY(writer.addFile(QString::fromUtf8("日本語/カテゴリ名.txt"), payload2));
	const QByteArray zipBytes = writer.finish();
	QVERIFY(writer.isValid());
	QVERIFY(!zipBytes.isEmpty());

	ZipReader reader(zipBytes);
	QVERIFY(reader.isValid());
	QCOMPARE(reader.fileNames().size(), 2);
	QVERIFY(reader.contains("ascii.txt"));
	QVERIFY(reader.contains(QString::fromUtf8("日本語/カテゴリ名.txt")));

	bool ok = false;
	const QByteArray read1 = reader.read("ascii.txt", &ok);
	QVERIFY(ok);
	QCOMPARE(read1, payload1);

	const QByteArray read2 = reader.read(QString::fromUtf8("日本語/カテゴリ名.txt"), &ok);
	QVERIFY(ok);
	QCOMPARE(read2, payload2);
}

void TestModelIo::documentPackageExportSkipsNonRedistributable() {
	Document doc;
	doc.title = QStringLiteral("パッケージテスト");
	doc.board = makeTestBoard();
	doc.dependencies.append(
		Dependency{QStringLiteral("pass-compat"), QStringLiteral("PasS互換"), QStringLiteral("0"), QString(), false});
	doc.dependencies.append(
		Dependency{QStringLiteral("cc0-lib"), QStringLiteral("CC0ライブラリ"), QStringLiteral("1.0"),
				   QStringLiteral("CC0-1.0"), true});

	auto passCompat = std::make_shared<Library>();
	passCompat->id = QStringLiteral("pass-compat");
	passCompat->name = QStringLiteral("PasS互換");
	passCompat->redistribution.allowed = false;  // 再配布不可

	auto cc0Lib = std::make_shared<Library>();
	cc0Lib->id = QStringLiteral("cc0-lib");
	cc0Lib->name = QStringLiteral("CC0ライブラリ");
	cc0Lib->license.kind = LicenseKind::CC0_1_0;
	cc0Lib->redistribution = redistributionRuleFor(LicenseKind::CC0_1_0);
	auto part = std::make_shared<Part>(makeTestPart());
	cc0Lib->parts.insert(part->id, part);
	cc0Lib->partCategory.insert(part->id, QStringLiteral("R"));

	documentio::LibraryResolver resolver = [&](const QString &id) -> std::shared_ptr<Library> {
		if (id == passCompat->id) return passCompat;
		if (id == cc0Lib->id) return cc0Lib;
		return nullptr;
	};

	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	const QString bpkgPath = dir.filePath("test.bpkg");
	// pass-compat は含めない (再配布不可のためユーザーが同梱を選ばなかった想定)。
	const QSet<QString> includeIds = {QStringLiteral("cc0-lib")};
	const auto result = documentio::exportPackage(doc, resolver, includeIds, bpkgPath);
	QVERIFY2(result.ok, qPrintable(result.error));
	QCOMPARE(result.bundledLibraryIds.size(), 1);
	QCOMPARE(result.bundledLibraryIds[0], QStringLiteral("cc0-lib"));
	QCOMPARE(result.skippedLibraryIds.size(), 1);
	QCOMPARE(result.skippedLibraryIds[0], QStringLiteral("pass-compat"));

	Document restoredDoc;
	QStringList importedLibIds;
	documentio::LibraryImporter importer = [&](const QString &id, const Library &) { importedLibIds.append(id); };
	const auto importResult = documentio::importPackage(bpkgPath, restoredDoc, importer);
	QVERIFY2(importResult.ok, qPrintable(importResult.error));
	QCOMPARE(restoredDoc.title, doc.title);
	QCOMPARE(importedLibIds.size(), 1);
	QCOMPARE(importedLibIds[0], QStringLiteral("cc0-lib"));
}

QTEST_MAIN(TestModelIo)
#include "test_model_io.moc"
