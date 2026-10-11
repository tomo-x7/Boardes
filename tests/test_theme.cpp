#include <QApplication>
#include <QImage>
#include <QLabel>
#include <QPalette>
#include <QTest>

#include "ui/theme.h"

// テーマ (ライト/ダーク) とアプリのパレット。QSS で文字色を指定していないところ (QLabel・一覧の項目・
// ツールチップなど) はパレットの色で描かれるので、パレットがテーマに追従していないと、ダークテーマで
// 暗い文字が暗い背景に重なって読めなくなる。テストの実行環境 (offscreen) のパレットはライトなので、
// 「OS はライトのままダークテーマを使う」状況をそのまま再現できる。
class TestTheme : public QObject {
	Q_OBJECT

private slots:
	void initTestCase();
	void cleanupTestCase();
	void paletteFollowsTheme();
	void labelTextIsReadableInDarkTheme();
};

void TestTheme::initTestCase() {
	Theme::instance().init();
}

void TestTheme::cleanupTestCase() {
	Theme::instance().forceTheme(false);
}

void TestTheme::paletteFollowsTheme() {
	struct Pair {
		QPalette::ColorRole fg;
		QPalette::ColorRole bg;
		int minContrast;  // 明度 (0〜255) の差
	};
	// 本文の文字は十分に、補足の文字 (プレースホルダ・palette(mid)) はそれなりに背景と離れていること。
	const QVector<Pair> pairs = {
		{QPalette::WindowText, QPalette::Window, 150},		{QPalette::Text, QPalette::Base, 150},
		{QPalette::ButtonText, QPalette::Button, 150},		{QPalette::HighlightedText, QPalette::Highlight, 150},
		{QPalette::ToolTipText, QPalette::ToolTipBase, 150}, {QPalette::PlaceholderText, QPalette::Base, 90},
		{QPalette::Mid, QPalette::Window, 90},
	};
	for (const bool dark : {false, true}) {
		Theme::instance().forceTheme(dark);
		const QPalette pal = qApp->palette();
		const int windowLightness = pal.color(QPalette::Window).lightness();
		QVERIFY2(dark ? windowLightness < 64 : windowLightness > 192, qPrintable(QString::number(windowLightness)));
		for (const Pair &pair : pairs) {
			const int contrast = qAbs(pal.color(pair.fg).lightness() - pal.color(pair.bg).lightness());
			const QString message =
				QString("dark=%1 role %2 / %3: contrast %4").arg(dark).arg(pair.fg).arg(pair.bg).arg(contrast);
			QVERIFY2(contrast >= pair.minContrast, qPrintable(message));
		}
	}
}

void TestTheme::labelTextIsReadableInDarkTheme() {
	// 統計パネルの数値などと同じ、色を指定していない QLabel。背景は暗く、文字は明るく描かれること。
	Theme::instance().forceTheme(true);
	QLabel label(QStringLiteral("配線長合計: 12.70mm"));
	QFont font = label.font();
	font.setPointSize(16);
	label.setFont(font);
	label.resize(label.sizeHint() + QSize(20, 20));
	const QImage image = label.grab().toImage();
	QVERIFY(!image.isNull());
	QVERIFY2(QColor(image.pixel(1, 1)).lightness() < 80, "背景が暗くない");
	int brightest = 0;
	for (int y = 0; y < image.height(); ++y) {
		for (int x = 0; x < image.width(); ++x) {
			brightest = qMax(brightest, QColor(image.pixel(x, y)).lightness());
		}
	}
	QVERIFY2(brightest > 180, qPrintable(QString("文字が暗い (最も明るい画素の明度 %1)").arg(brightest)));
}

QTEST_MAIN(TestTheme)
#include "test_theme.moc"
