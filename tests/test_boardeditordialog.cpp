#include <QApplication>
#include <QDialogButtonBox>
#include <QImage>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QTest>
#include <QTimer>

#include "core/units.h"
#include "ui/boardeditordialog.h"

namespace {
// 「画像から作る」/「パラメトリック」ラジオボタンはダイアログの private メンバなので、
// テストからはテキストで名前引きする。
QRadioButton *radioWithText(const QDialog &dialog, const QString &text) {
	for (QRadioButton *r : dialog.findChildren<QRadioButton *>()) {
		if (r->text() == text) {
			return r;
		}
	}
	return nullptr;
}
}  // namespace

class TestBoardEditorDialog : public QObject {
	Q_OBJECT

private slots:
	void defaultsAreSensible();
	void setBoardThenBoardRoundTrips();
	void sizeIsDerivedFromGridParameters();
	void backgroundImageRoundTripsAndDrivesSize();
	void clearingBackgroundImageRevertsToGridDerivedSize();
	void imageModeForcesParametricFieldsToDefaults();
	void creatingWithoutFrontImageInImageModeIsRejected();
	void titleAndOkButtonReflectCreateVsEditMode();

private:
	static BoardSpec makeSampleBoard();
};

BoardSpec TestBoardEditorDialog::makeSampleBoard() {
	BoardSpec b;
	b.id = "sample";
	b.name = "サンプル基板";
	b.cols = 8;
	b.rows = 6;
	b.pitch = 10;
	b.origin = QPoint(5, 5);
	b.padShape = PadShape::Square;
	b.padDiameter = 7;
	b.holeDiameter = 4;
	b.copper = CopperPattern::StripHorizontal;
	b.doubleSided = true;
	b.substrateColor = QColor(10, 20, 30);
	b.padColor = QColor(40, 50, 60);
	b.copperColor = QColor(70, 80, 90);
	return b;
}

void TestBoardEditorDialog::defaultsAreSensible() {
	BoardEditorDialog dialog;
	const BoardSpec b = dialog.board();
	QCOMPARE(b.cols, 10);
	QCOMPARE(b.rows, 10);
	QCOMPARE(b.pitch, units::Pitch);
	QCOMPARE(static_cast<int>(b.padShape), static_cast<int>(PadShape::Round));
	QCOMPARE(static_cast<int>(b.copper), static_cast<int>(CopperPattern::PadPerHole));
	QVERIFY(!b.doubleSided);
	// 新規状態では id/名前は空 (呼び出し側が入力を促す)。
	QVERIFY(b.id.isEmpty());
	QVERIFY(b.name.isEmpty());
	// 既定の基板色は Boardes 独自の色であり、PasS のクロマキー色ではない
	// (board.h / boardio.cpp / boardeditordialog.h の3箇所が食い違っていないことの確認)。
	QCOMPARE(b.substrateColor, boarddefaults::Substrate);
	QVERIFY(!b.backgroundFront.has_value());
	QVERIFY(!b.backgroundBack.has_value());
}

void TestBoardEditorDialog::setBoardThenBoardRoundTrips() {
	BoardEditorDialog dialog;
	const BoardSpec original = makeSampleBoard();
	dialog.setBoard(original);

	const BoardSpec result = dialog.board();
	QCOMPARE(result.id, original.id);
	QCOMPARE(result.name, original.name);
	QCOMPARE(result.cols, original.cols);
	QCOMPARE(result.rows, original.rows);
	QCOMPARE(result.pitch, original.pitch);
	QCOMPARE(result.origin, original.origin);
	QCOMPARE(static_cast<int>(result.padShape), static_cast<int>(original.padShape));
	QCOMPARE(result.padDiameter, original.padDiameter);
	QCOMPARE(result.holeDiameter, original.holeDiameter);
	QCOMPARE(static_cast<int>(result.copper), static_cast<int>(original.copper));
	QCOMPARE(result.doubleSided, original.doubleSided);
	QCOMPARE(result.substrateColor, original.substrateColor);
	QCOMPARE(result.padColor, original.padColor);
	QCOMPARE(result.copperColor, original.copperColor);
}

void TestBoardEditorDialog::sizeIsDerivedFromGridParameters() {
	BoardEditorDialog dialog;
	BoardSpec b = makeSampleBoard();  // cols=8, rows=6, pitch=10, origin=(5,5)
	dialog.setBoard(b);

	const BoardSpec result = dialog.board();
	// size = 2*origin + (cols/rows-1)*pitch (対称マージンでの自動導出)。
	QCOMPARE(result.size, QSize(2 * 5 + (8 - 1) * 10, 2 * 5 + (6 - 1) * 10));
}

void TestBoardEditorDialog::backgroundImageRoundTripsAndDrivesSize() {
	BoardEditorDialog dialog;
	BoardSpec b = makeSampleBoard();

	QImage front(120, 90, QImage::Format_ARGB32);
	front.fill(Qt::green);
	QImage back(120, 90, QImage::Format_ARGB32);
	back.fill(Qt::blue);
	b.backgroundFront = Artwork::fromImageAsIs(front);
	b.backgroundBack = Artwork::fromImageAsIs(back);

	dialog.setBoard(b);
	const BoardSpec result = dialog.board();

	QVERIFY(result.backgroundFront.has_value());
	QVERIFY(result.backgroundBack.has_value());
	QCOMPARE(result.backgroundFront->image.size(), front.size());
	QCOMPARE(result.backgroundBack->image.size(), back.size());
	// 表面画像がある場合、基板サイズは格子ではなく画像の画素サイズに従う。
	QCOMPARE(result.size, front.size());
}

void TestBoardEditorDialog::clearingBackgroundImageRevertsToGridDerivedSize() {
	BoardEditorDialog dialog;
	BoardSpec b = makeSampleBoard();  // cols=8, rows=6, pitch=10, origin=(5,5)
	QImage front(120, 90, QImage::Format_ARGB32);
	front.fill(Qt::green);
	b.backgroundFront = Artwork::fromImageAsIs(front);
	dialog.setBoard(b);
	QCOMPARE(dialog.board().size, front.size());

	// クリアボタンに対応する private slot を直接呼ぶ (このダイアログの既存テストは
	// QTest のマウスイベントではなく setBoard/board を直接呼ぶ形式のため、それに倣う)。
	QMetaObject::invokeMethod(&dialog, "onClearFrontBackground", Qt::DirectConnection);

	const BoardSpec result = dialog.board();
	QVERIFY(!result.backgroundFront.has_value());
	QCOMPARE(result.size, QSize(2 * 5 + (8 - 1) * 10, 2 * 5 + (6 - 1) * 10));
}

void TestBoardEditorDialog::imageModeForcesParametricFieldsToDefaults() {
	BoardEditorDialog dialog;
	dialog.setBoard(makeSampleBoard());  // 画像なしなのでパラメトリックモードになる
	QCOMPARE(static_cast<int>(dialog.board().padShape), static_cast<int>(PadShape::Square));  // 前提の確認

	auto *imageModeRadio = radioWithText(dialog, QStringLiteral("画像から作る"));
	QVERIFY(imageModeRadio != nullptr);
	imageModeRadio->setChecked(true);

	// 画像モードに切り替えると、パッド形状・銅箔パターン・両面・色は既定値に強制される
	// (背景画像がすべての見た目を担うため)。
	const BoardSpec result = dialog.board();
	QCOMPARE(static_cast<int>(result.padShape), static_cast<int>(PadShape::None));
	QCOMPARE(static_cast<int>(result.copper), static_cast<int>(CopperPattern::None));
	QVERIFY(!result.doubleSided);
	QCOMPARE(result.substrateColor, boarddefaults::Substrate);
	QCOMPARE(result.padColor, boarddefaults::Pad);
	QCOMPARE(result.copperColor, boarddefaults::Copper);
}

void TestBoardEditorDialog::creatingWithoutFrontImageInImageModeIsRejected() {
	BoardEditorDialog dialog;
	auto *idEdit = dialog.findChild<QLineEdit *>();
	QVERIFY(idEdit != nullptr);
	// 1つ目の QLineEdit は ID 欄 (コンストラクタでの追加順)。
	idEdit->setText(QStringLiteral("no-image-board"));
	auto lineEdits = dialog.findChildren<QLineEdit *>();
	QVERIFY(lineEdits.size() >= 2);
	lineEdits[1]->setText(QStringLiteral("画像なし基板"));  // 名前欄

	auto *imageModeRadio = radioWithText(dialog, QStringLiteral("画像から作る"));
	QVERIFY(imageModeRadio != nullptr);
	imageModeRadio->setChecked(true);

	// onAccept() は検証に失敗すると QMessageBox::warning (モーダル) を出す。
	// テストでは自動的に閉じる。
	QTimer::singleShot(0, [] {
		if (QWidget *modal = QApplication::activeModalWidget()) {
			modal->close();
		}
	});
	QMetaObject::invokeMethod(&dialog, "onAccept", Qt::DirectConnection);
	QVERIFY(dialog.result() != QDialog::Accepted);
}

void TestBoardEditorDialog::titleAndOkButtonReflectCreateVsEditMode() {
	BoardEditorDialog dialog;
	auto *okButton = dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
	QVERIFY(okButton != nullptr);
	QCOMPARE(dialog.windowTitle(), QStringLiteral("基板を作成"));
	QCOMPARE(okButton->text(), QStringLiteral("作成"));

	dialog.setBoard(makeSampleBoard());
	QCOMPARE(dialog.windowTitle(), QStringLiteral("基板を編集"));
	QCOMPARE(okButton->text(), QStringLiteral("保存"));
}

QTEST_MAIN(TestBoardEditorDialog)
#include "test_boardeditordialog.moc"
