#include <QImage>
#include <QTableWidget>
#include <QTest>

#include "ui/parteditordialog.h"

// PartArtworkCanvas への本物の QTest::mouseClick で「クリックしてピンを打つ」動線を
// 検証する (シグナル配線だけのテストだと、クリック座標→画像座標の変換ミスや
// スケール計算のバグを見逃すため)。
class TestPartEditorDialog : public QObject {
	Q_OBJECT

private slots:
	void setPartThenPartRoundTripsMetadataAndPins();
	void clickingCanvasAddsNumberedPin();
	void rightClickRemovesNearestPinAndKeepsNumbersSequential();

private:
	static Part makeSamplePart();
};

Part TestPartEditorDialog::makeSamplePart() {
	Part p;
	p.id = "R-2";
	p.name = QStringLiteral("抵抗 2グリッド");
	p.kind = PartKind::Normal;
	p.refPrefix = "R";
	p.keywords = {"resistor", QStringLiteral("抵抗")};
	QImage img(25, 7, QImage::Format_RGB888);
	img.fill(Qt::gray);
	p.artwork = Artwork::fromImageAsIs(img);
	p.pins = {Pin{1, QPoint(2, 3), 0, {}}, Pin{2, QPoint(22, 3), 0, {}}};
	return p;
}

void TestPartEditorDialog::setPartThenPartRoundTripsMetadataAndPins() {
	PartEditorDialog dialog;
	const Part original = makeSamplePart();
	dialog.setPart(original);

	const Part result = dialog.part();
	QCOMPARE(result.id, original.id);
	QCOMPARE(result.name, original.name);
	QCOMPARE(static_cast<int>(result.kind), static_cast<int>(original.kind));
	QCOMPARE(result.refPrefix, original.refPrefix);
	QCOMPARE(result.keywords, original.keywords);
	QCOMPARE(result.pins.size(), 2);
	QCOMPARE(result.pins[0].pos, QPoint(2, 3));
	QCOMPARE(result.pins[0].number, 1);
	QCOMPARE(result.pins[1].pos, QPoint(22, 3));
	QCOMPARE(result.pins[1].number, 2);

	auto *table = dialog.findChild<QTableWidget *>();
	QVERIFY(table != nullptr);
	QCOMPARE(table->rowCount(), 2);
}

void TestPartEditorDialog::clickingCanvasAddsNumberedPin() {
	PartEditorDialog dialog;
	Part p = makeSamplePart();
	p.pins.clear();
	dialog.setPart(p);  // 画像だけセットし、ピンが空の状態から始める
	QCOMPARE(dialog.part().pins.size(), 0);

	auto *canvas = dialog.findChild<PartArtworkCanvas *>();
	QVERIFY(canvas != nullptr);
	// canvas は QScrollArea 内の子ウィジェットなので、露出待ちはトップレベルの
	// dialog に対して行う必要がある (子だけ show() してもトップレベルが非表示のままだと
	// qWaitForWindowExposed は真の露出を検出できない)。
	dialog.show();
	QVERIFY(QTest::qWaitForWindowExposed(&dialog));

	// キャンバスは画像を整数倍率で拡大表示しているので、正確な倍率に依存しないよう
	// ウィジェット中央をクリックする (どんな倍率でも画像内の有効な位置になる)。
	QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, canvas->rect().center());

	const auto pins = dialog.part().pins;
	QCOMPARE(pins.size(), 1);
	QCOMPARE(pins[0].number, 1);  // 既定は番号あり (ドリル穴のみモードは既定オフ)

	auto *table = dialog.findChild<QTableWidget *>();
	QVERIFY(table != nullptr);
	QCOMPARE(table->rowCount(), 1);
}

void TestPartEditorDialog::rightClickRemovesNearestPinAndKeepsNumbersSequential() {
	PartEditorDialog dialog;
	Part p = makeSamplePart();
	p.pins.clear();
	dialog.setPart(p);

	auto *canvas = dialog.findChild<PartArtworkCanvas *>();
	QVERIFY(canvas != nullptr);
	// canvas は QScrollArea 内の子ウィジェットなので、露出待ちはトップレベルの
	// dialog に対して行う必要がある (子だけ show() してもトップレベルが非表示のままだと
	// qWaitForWindowExposed は真の露出を検出できない)。
	dialog.show();
	QVERIFY(QTest::qWaitForWindowExposed(&dialog));

	const QPoint center = canvas->rect().center();
	QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, center);
	QCOMPARE(dialog.part().pins.size(), 1);

	QTest::mouseClick(canvas, Qt::RightButton, Qt::NoModifier, center);
	QCOMPARE(dialog.part().pins.size(), 0);

	auto *table = dialog.findChild<QTableWidget *>();
	QVERIFY(table != nullptr);
	QCOMPARE(table->rowCount(), 0);
}

QTEST_MAIN(TestPartEditorDialog)
#include "test_parteditordialog.moc"
