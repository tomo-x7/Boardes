#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QTest>
#include <algorithm>
#include <cmath>

#include "io/passimport.h"
#include "model/library.h"

namespace {

void putU16(QByteArray &out, uint16_t v) {
	out.append(static_cast<char>(v & 0xFF));
	out.append(static_cast<char>((v >> 8) & 0xFF));
}
void putU32(QByteArray &out, uint32_t v) {
	out.append(static_cast<char>(v & 0xFF));
	out.append(static_cast<char>((v >> 8) & 0xFF));
	out.append(static_cast<char>((v >> 16) & 0xFF));
	out.append(static_cast<char>((v >> 24) & 0xFF));
}

// PasS の実物の .ico と同じ「クラシック DIB (ファイルヘッダ無し) + 8bpp パレット +
// AND マスク」形式の最小限の ICO バイト列を合成する。
//
// 実物の R.ico をバイナリ解析して2つ確認した点を反映している:
//   1. biBitCount=8 (256色パレット)。最初 24bpp 前提で実装し、実データでのみ
//      icon=null になる不具合を作り込んだことがある。
//   2. 実物はほぼ単色 (パレット index 0 のみ) で、形そのものは AND マスクが
//      定義している。マスク適用を「あれば良い程度」として省いた結果、
//      実データでは全面べた塗りの正方形になる不具合を作り込んだことがある。
// そのため、このフィクスチャは最下段の格納行 (=最終画像の一番下の行) だけを
// AND マスクで透明にし、それ以外を不透明にする。マスクを無視する実装だと
// 透明になるべき画素が不透明のまま描画され、テストで検出できる。
QByteArray makeSyntheticIco(int size, QColor fill) {
	QByteArray out;
	putU16(out, 0);  // reserved
	putU16(out, 1);  // type = icon
	putU16(out, 1);  // count = 1

	const int paletteEntries = 256;
	const int paletteBytes = paletteEntries * 4;
	const int xorStride = ((size * 8 + 31) / 32) * 4;  // 8bpp, 4byte境界パディング
	const int andStride = ((size * 1 + 31) / 32) * 4;  // 1bpp, 4byte境界パディング
	const uint32_t headerSize = 40;
	const uint32_t xorBytes = static_cast<uint32_t>(xorStride * size);
	const uint32_t andBytes = static_cast<uint32_t>(andStride * size);
	const uint32_t imgSize = headerSize + paletteBytes + xorBytes + andBytes;
	const uint32_t imgOffset = 6 + 16;

	out.append(static_cast<char>(size == 256 ? 0 : size));
	out.append(static_cast<char>(size == 256 ? 0 : size));
	out.append(static_cast<char>(0));
	out.append(static_cast<char>(0));
	putU16(out, 1);
	putU16(out, 8);
	putU32(out, imgSize);
	putU32(out, imgOffset);

	putU32(out, headerSize);
	putU32(out, static_cast<uint32_t>(size));
	putU32(out, static_cast<uint32_t>(size * 2));  // XOR+AND 合計 (実高さはこの半分)
	putU16(out, 1);
	putU16(out, 8);
	putU32(out, 0);  // BI_RGB
	putU32(out, xorBytes);
	putU32(out, 0);
	putU32(out, 0);
	putU32(out, 0);  // biClrUsed=0 -> 256色パレット全部使用
	putU32(out, 0);

	// パレット: index 0 = fill 色、それ以外は黒。
	out.append(static_cast<char>(fill.blue()));
	out.append(static_cast<char>(fill.green()));
	out.append(static_cast<char>(fill.red()));
	out.append(static_cast<char>(0));
	for (int i = 1; i < paletteEntries; ++i) {
		out.append(4, static_cast<char>(0));
	}

	const QByteArray xorRow(xorStride, static_cast<char>(0));  // 全画素インデックス0 (= fill 色)
	for (int y = 0; y < size; ++y) {
		out.append(xorRow);
	}

	// AND マスク: ボトムアップ格納の先頭行 (= 最終画像の一番下の行) だけ全ビット1 (透明)、
	// それ以外は全ビット0 (不透明)。
	const QByteArray transparentRow(andStride, static_cast<char>(0xFF));
	const QByteArray opaqueRow(andStride, static_cast<char>(0x00));
	out.append(transparentRow);
	for (int y = 1; y < size; ++y) {
		out.append(opaqueRow);
	}
	return out;
}

void drawFilledCircle(QImage &img, QPoint center, int radius, QColor color) {
	for (int y = -radius; y <= radius; ++y) {
		for (int x = -radius; x <= radius; ++x) {
			if (x * x + y * y <= radius * radius) {
				const int px = center.x() + x;
				const int py = center.y() + y;
				if (px >= 0 && py >= 0 && px < img.width() && py < img.height()) {
					img.setPixelColor(px, py, color);
				}
			}
		}
	}
}

QString writeBmp(const QString &dir, const QString &fileName, const QImage &img) {
	const QString path = dir + QLatin1Char('/') + fileName;
	img.save(path, "BMP");
	return path;
}

}  // namespace

class TestPassImport : public QObject {
	Q_OBJECT

private slots:
	void decodesSimplePinPositions();
	void importedPartDoesNotRecordChromaKeyButStaysTransparent();
	void decodesDipStylePinOrder();
	void decodesDrillOnlyCodes();
	void decodesCombinedPinAndDrill();
	void decodesShiftJisCategoryName();
	void decodesBothIcoAndBmpDisguisedAsIco();
	void detectsBoardGridAndMountingHoles();
	void skipsNonBmpFilesAndReportsMissingBackBoard();
	void realPassDataIfAvailable();
};

void TestPassImport::decodesSimplePinPositions() {
	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	QVERIFY(QDir(dir.path()).mkpath("R"));

	// 実物の R-2.bmp (25x7, ピン1@(2,3)=RGB(254,0,0), ピン2@(22,3)=RGB(253,0,0)、両方 drill=0) を
	// 再現したテスト画像。number = 255-R なので R=255 ちょうどは「番号なし」になる点に注意
	// (R=254 → 1、R=253 → 2)。
	QImage img(25, 7, QImage::Format_RGB888);
	img.fill(QColor(187, 201, 158));
	img.setPixelColor(2, 3, QColor(254, 0, 0));   // pin1, drill=0
	img.setPixelColor(22, 3, QColor(253, 0, 0));  // pin2, drill=0
	writeBmp(dir.path() + "/R", "R-2.bmp", img);

	Library lib;
	const auto result = passimport::importFromDirectory(dir.path(), lib);
	QVERIFY2(result.ok, qPrintable(result.error));
	QCOMPARE(result.partCount, 1);
	QVERIFY(lib.parts.contains("R-2"));

	const auto &part = *lib.parts["R-2"];
	QCOMPARE(part.pins.size(), 2);
	QCOMPARE(part.refPrefix, QStringLiteral("R"));
	auto pin1 = std::find_if(part.pins.begin(), part.pins.end(), [](const Pin &p) { return p.pos == QPoint(2, 3); });
	auto pin2 = std::find_if(part.pins.begin(), part.pins.end(), [](const Pin &p) { return p.pos == QPoint(22, 3); });
	QVERIFY(pin1 != part.pins.end());
	QVERIFY(pin2 != part.pins.end());
	QCOMPARE(pin1->number, 1);
	QCOMPARE(pin1->drill, 0);
	QCOMPARE(pin2->number, 2);
	QCOMPARE(pin2->drill, 0);

	// マーカー画素はもう近傍色に埋め戻さない (Phase 13)。元のマーカー色のまま、
	// 不透明で残っているはず (Boardes 側の「接点マーカー表示」機能がこの上に重ねて
	// 表示する前提のため)。
	QCOMPARE(part.artwork.image.pixelColor(2, 3), QColor(254, 0, 0));
	QCOMPARE(part.artwork.image.pixelColor(2, 3).alpha(), 255);
}

void TestPassImport::importedPartDoesNotRecordChromaKeyButStaysTransparent() {
	// 「PasS 資産は読み込んで使えれば十分で、そのまま (PasS の色を保持して) 使える必要は
	// 無い」という方針の確認。透過処理自体 (画素のアルファ) は効いたままであることも
	// 同時に固定し、「メタデータは消えたが変換も壊れた」という回帰を防ぐ。
	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	QVERIFY(QDir(dir.path()).mkpath("R"));

	QImage img(25, 7, QImage::Format_RGB888);
	img.fill(QColor(187, 201, 158));
	img.setPixelColor(2, 3, QColor(254, 0, 0));
	writeBmp(dir.path() + "/R", "R-3.bmp", img);

	Library lib;
	const auto result = passimport::importFromDirectory(dir.path(), lib);
	QVERIFY2(result.ok, qPrintable(result.error));
	QVERIFY(lib.parts.contains("R-3"));

	const auto &part = *lib.parts["R-3"];
	QVERIFY(!part.artwork.chromaKey.has_value());
	QCOMPARE(part.artwork.image.pixelColor(0, 0).alpha(), 0);  // 背景色の画素は透過済み
}

void TestPassImport::decodesDipStylePinOrder() {
	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	QVERIFY(QDir(dir.path()).mkpath("IC"));

	// 実物の ICD-14.bmp (69x35) を模した DIP14 配列: 下段(y=32) 1-7, 上段(y=2) 8-14。
	QImage img(69, 35, QImage::Format_RGB888);
	img.fill(QColor(187, 201, 158));
	for (int i = 0; i < 7; ++i) {
		img.setPixelColor(4 + i * 10, 32, QColor(255 - (i + 1), 0, 0));  // pin (i+1)
		img.setPixelColor(4 + i * 10, 2, QColor(255 - (i + 8), 0, 0));   // pin (i+8)
	}
	writeBmp(dir.path() + "/IC", "ICD-14.bmp", img);

	Library lib;
	const auto result = passimport::importFromDirectory(dir.path(), lib);
	QVERIFY2(result.ok, qPrintable(result.error));
	QVERIFY(lib.parts.contains("ICD-14"));
	const auto &part = *lib.parts["ICD-14"];
	QCOMPARE(part.pins.size(), 14);

	for (int i = 0; i < 7; ++i) {
		const QPoint bottom(4 + i * 10, 32);
		const QPoint top(4 + i * 10, 2);
		auto bIt = std::find_if(part.pins.begin(), part.pins.end(), [&](const Pin &p) { return p.pos == bottom; });
		auto tIt = std::find_if(part.pins.begin(), part.pins.end(), [&](const Pin &p) { return p.pos == top; });
		QVERIFY(bIt != part.pins.end());
		QVERIFY(tIt != part.pins.end());
		QCOMPARE(bIt->number, i + 1);
		QCOMPARE(tIt->number, i + 8);
	}
}

void TestPassImport::decodesDrillOnlyCodes() {
	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	QVERIFY(QDir(dir.path()).mkpath("Hole"));

	auto makeHole = [&](const QString &name, int r, int g, int b) {
		QImage img(5, 5, QImage::Format_RGB888);
		img.fill(QColor(187, 201, 158));
		img.setPixelColor(2, 2, QColor(r, g, b));
		writeBmp(dir.path() + "/Hole", name, img);
	};
	makeHole("Hole-C08.bmp", 255, 8, 0);
	makeHole("Hole-C30.bmp", 255, 14, 1);   // 1*16+14 = 30
	makeHole("Hole-C32.bmp", 255, 0, 2);    // 2*16+0 = 32

	Library lib;
	const auto result = passimport::importFromDirectory(dir.path(), lib);
	QVERIFY2(result.ok, qPrintable(result.error));
	QCOMPARE(result.partCount, 3);

	QVERIFY(lib.parts.contains("Hole-C08"));
	QCOMPARE(lib.parts["Hole-C08"]->pins.size(), 1);
	QCOMPARE(lib.parts["Hole-C08"]->pins[0].number, 0);
	QCOMPARE(lib.parts["Hole-C08"]->pins[0].drill, 8);
	QCOMPARE(lib.parts["Hole-C08"]->kind, PartKind::DrillHole);

	QCOMPARE(lib.parts["Hole-C30"]->pins[0].drill, 30);
	QCOMPARE(lib.parts["Hole-C32"]->pins[0].drill, 32);
}

void TestPassImport::decodesCombinedPinAndDrill() {
	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	QVERIFY(QDir(dir.path()).mkpath("CN"));

	// 実物の USB-A.bmp の (250,4,1) = ピン5 かつ drill=1*16+4=20 (2.0mm) を再現。
	QImage img(20, 20, QImage::Format_RGB888);
	img.fill(QColor(187, 201, 158));
	img.setPixelColor(10, 10, QColor(250, 4, 1));
	writeBmp(dir.path() + "/CN", "USB-A.bmp", img);

	Library lib;
	const auto result = passimport::importFromDirectory(dir.path(), lib);
	QVERIFY2(result.ok, qPrintable(result.error));
	QVERIFY(lib.parts.contains("USB-A"));
	const auto &pin = lib.parts["USB-A"]->pins[0];
	QCOMPARE(pin.number, 5);
	QCOMPARE(pin.drill, 20);
}

void TestPassImport::decodesShiftJisCategoryName() {
	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	QVERIFY(QDir(dir.path()).mkpath("R"));

	QFile txt(dir.path() + "/R/R.txt");
	QVERIFY(txt.open(QIODevice::WriteOnly));
	txt.write(QByteArray::fromHex("92ef8d52"));  // Shift-JIS で「抵抗」
	txt.close();

	// カテゴリとして認識されるには最低1部品必要、というわけではないが空でも通ることを確認するため
	// あえて部品を置かない。
	Library lib;
	const auto result = passimport::importFromDirectory(dir.path(), lib);
	QVERIFY2(result.ok, qPrintable(result.error));
	QCOMPARE(result.categoryCount, 1);
	const auto *cat = lib.category("R");
	QVERIFY(cat != nullptr);
	QCOMPARE(cat->name, QString::fromUtf8("抵抗"));
}

void TestPassImport::decodesBothIcoAndBmpDisguisedAsIco() {
	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	QVERIFY(QDir(dir.path()).mkpath("R"));
	QVERIFY(QDir(dir.path()).mkpath("Tool"));

	// R.ico: 本物の ICO (実物の R.ico と同じ形式)。
	QFile realIco(dir.path() + "/R/R.ico");
	QVERIFY(realIco.open(QIODevice::WriteOnly));
	realIco.write(makeSyntheticIco(16, QColor(200, 50, 50)));
	realIco.close();

	// Tool.ico: 実体は BMP なのに拡張子が .ico (実物の Tool.ico と同じ状況)。
	QImage bmpImg(16, 16, QImage::Format_RGB888);
	bmpImg.fill(QColor(50, 50, 200));
	QByteArray bmpBytes;
	{
		QBuffer buf(&bmpBytes);
		buf.open(QIODevice::WriteOnly);
		bmpImg.save(&buf, "BMP");
	}
	QFile fakeIco(dir.path() + "/Tool/Tool.ico");
	QVERIFY(fakeIco.open(QIODevice::WriteOnly));
	fakeIco.write(bmpBytes);
	fakeIco.close();

	Library lib;
	const auto result = passimport::importFromDirectory(dir.path(), lib);
	QVERIFY2(result.ok, qPrintable(result.error));

	const auto *rCat = lib.category("R");
	const auto *toolCat = lib.category("Tool");
	QVERIFY(rCat != nullptr);
	QVERIFY(toolCat != nullptr);
	QVERIFY(!rCat->icon.isNull());
	QVERIFY(!toolCat->icon.isNull());
	QCOMPARE(rCat->icon.width(), 16);
	QCOMPARE(rCat->icon.height(), 16);
	QCOMPARE(toolCat->icon.width(), 16);
	QCOMPARE(toolCat->icon.height(), 16);

	// R.ico の合成フィクスチャは最終画像の一番下の行だけ AND マスクで透明にしてある。
	// マスクを無視する実装だとここが不透明な塗り色のまま残るため、実データ (ほぼ単色+
	// AND マスクだけで形を表現する古いアイコン) で全面べた塗りになる不具合を検出できる。
	QCOMPARE(rCat->icon.pixelColor(0, 15).alpha(), 0);
	QCOMPARE(rCat->icon.pixelColor(0, 0).alpha(), 255);
	QCOMPARE(rCat->icon.pixelColor(0, 0), QColor(200, 50, 50));
}

void TestPassImport::detectsBoardGridAndMountingHoles() {
	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	QVERIFY(QDir(dir.path()).mkpath("Board"));

	const int w = 90, h = 70;
	QImage front(w, h, QImage::Format_RGB888);
	front.fill(QColor(187, 201, 158));

	// 6x4 グリッド (pitch=10, origin=(10,10))。(col=2,row=1) だけ穴を開けない。
	for (int row = 0; row < 4; ++row) {
		for (int col = 0; col < 6; ++col) {
			if (col == 2 && row == 1) continue;
			const int cx = 10 + col * 10;
			const int cy = 10 + row * 10;
			for (int dy = -1; dy <= 1; ++dy) {
				for (int dx = -1; dx <= 1; ++dx) {
					front.setPixelColor(cx + dx, cy + dy, QColor(255, 255, 255));
				}
			}
		}
	}
	// 取付穴 (グリッドより明らかに大きい白ブロブ)。
	drawFilledCircle(front, QPoint(80, 20), 6, QColor(255, 255, 255));
	// 幅マーカー。
	front.setPixelColor(5, 0, QColor(0, 0, 0));
	front.setPixelColor(85, 0, QColor(0, 0, 0));

	QImage back(w, h, QImage::Format_RGB888);
	back.fill(QColor(200, 160, 90));

	writeBmp(dir.path() + "/Board", "TESTB.bmp", front);
	writeBmp(dir.path() + "/Board", "TESTB_.bmp", back);

	Library lib;
	const auto result = passimport::importFromDirectory(dir.path(), lib);
	QVERIFY2(result.ok, qPrintable(result.error));
	QCOMPARE(result.boardCount, 1);
	QVERIFY(lib.boards.contains("TESTB"));

	const auto &board = *lib.boards["TESTB"];
	QCOMPARE(board.cols, 6);
	QCOMPARE(board.rows, 4);
	QCOMPARE(board.pitch, 10);
	QCOMPARE(board.origin, QPoint(10, 10));
	QCOMPARE(board.absentHoles.size(), 1);
	QCOMPARE(board.absentHoles[0], QPoint(2, 1));
	QCOMPARE(board.mountingHoles.size(), 1);
	QVERIFY(std::abs(board.mountingHoles[0].second - 12) <= 2);  // 半径6 -> 直径約12
	QCOMPARE(board.padShape, PadShape::None);
	QCOMPARE(board.copper, CopperPattern::None);
	QVERIFY(board.outlineRect.has_value());
	QCOMPARE(*board.outlineRect, QRect(5, 0, 81, h));
	QVERIFY(board.backgroundFront.has_value());
	QVERIFY(board.backgroundBack.has_value());
	// 基板色は BMP の最頻色 (= PasS のクロマキー色そのもの) を焼き込まず、Boardes の
	// 既定色のままにする (背景画像が見た目を担うので描画には影響しない)。
	QCOMPARE(board.substrateColor, boarddefaults::Substrate);
}

void TestPassImport::skipsNonBmpFilesAndReportsMissingBackBoard() {
	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	QVERIFY(QDir(dir.path()).mkpath("Board"));
	QVERIFY(QDir(dir.path()).mkpath("R"));

	QImage front(20, 20, QImage::Format_RGB888);
	front.fill(QColor(187, 201, 158));
	front.setPixelColor(10, 10, QColor(255, 255, 255));
	writeBmp(dir.path() + "/Board", "NOBACK.bmp", front);  // 対応する _ ファイルを作らない

	QFile thumbs(dir.path() + "/R/Thumbs.db");
	QVERIFY(thumbs.open(QIODevice::WriteOnly));
	thumbs.write("not a part");
	thumbs.close();

	Library lib;
	const auto result = passimport::importFromDirectory(dir.path(), lib);
	QVERIFY2(result.ok, qPrintable(result.error));
	QCOMPARE(result.boardCount, 0);
	QCOMPARE(result.partCount, 0);
	QVERIFY(!result.issues.isEmpty());
	QVERIFY(std::any_of(result.issues.begin(), result.issues.end(),
						[](const passimport::ImportIssue &i) { return i.file == QStringLiteral("NOBACK.bmp"); }));
}

void TestPassImport::realPassDataIfAvailable() {
	const QString dir = qEnvironmentVariable("BOARDES_PASS_PARTS_DIR");
	if (dir.isEmpty()) {
		QSKIP("BOARDES_PASS_PARTS_DIR が未設定のためスキップします");
	}

	Library lib;
	const auto result = passimport::importFromDirectory(dir, lib);
	QVERIFY2(result.ok, qPrintable(result.error));

	qDebug() << "categories:" << result.categoryCount << "parts:" << result.partCount
			 << "boards:" << result.boardCount << "issues:" << result.issues.size();
	for (const auto &issue : result.issues) {
		qDebug() << "  issue:" << issue.file << issue.reason;
	}

	QCOMPARE(result.categoryCount, 19);  // Board を除く 19 カテゴリ
	QVERIFY(result.partCount > 200 && result.partCount < 260);
	QCOMPARE(result.boardCount, 11);

	QVERIFY(lib.parts.contains("R-2"));
	const auto &r2 = *lib.parts["R-2"];
	QCOMPARE(r2.pins.size(), 2);

	QVERIFY(lib.boards.contains("ICB-504"));
	const auto &icb504 = *lib.boards["ICB-504"];
	QVERIFY(icb504.cols > 0 && icb504.rows > 0);
	QCOMPARE(icb504.pitch, 10);
}

QTEST_MAIN(TestPassImport)
#include "test_passimport.moc"
