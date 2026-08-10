// Phase 8 (claude.ai/design 連携) 用のスクリーンショット採取ハーネス。
// boardes_core (ライブラリ本体) に対して単発でリンクする、テストスイートの一部ではない
// 開発ツール。実アプリの各画面を実際に構築・表示・グラブして design/screenshots/*.png に
// 書き出す。CMake のビルドツリーには含めない (テストではないため)。
// Phase 19以前の完了時点で、Phase 10〜19 の UI 変更 (ライブラリ管理の全面刷新、操作/
// ツールバーのカスタマイズ画面の新規追加等) に合わせて更新した。
// Phase 20 (claude.ai/design のデザイン刷新の実装) で Theme::instance().init() の呼び出しを
// 追加し、刷新後の配色・フォントで撮れるようにした。
//
// ビルド (プロジェクトルートで、事前に `cmake --build build` 済みであること):
//   g++ -DQT_CORE_LIB -DQT_GUI_LIB -DQT_SVG_LIB -DQT_WIDGETS_LIB -DQT_TESTLIB_LIB \
//     -I build/boardes_core_autogen/include -I src \
//     -isystem /usr/include/qt6/QtWidgets -isystem /usr/include/qt6 -isystem /usr/include/qt6/QtCore \
//     -isystem /usr/lib/qt6/mkspecs/linux-g++ -isystem /usr/include/qt6/QtGui -isystem /usr/include/qt6/QtSvg \
//     -isystem /usr/include/qt6/QtTest \
//     -fPIC -g -std=gnu++23 \
//     design/screenshot_all.cpp -o /tmp/screenshot_all \
//     build/libboardes_core.a -lQt6Widgets -lQt6Svg -lQt6Gui -lQt6Core -lQt6Test
//
// 実行 (実データを使うので QT_QPA_PLATFORM=offscreen 必須、XDG_DATA_HOME は隔離推奨):
//   XDG_DATA_HOME=$(mktemp -d) QT_QPA_PLATFORM=offscreen /tmp/screenshot_all
// ダークテーマで撮る場合は環境変数 BOARDES_SCREENSHOT_THEME_DARK=1 を追加する
// (design/screenshots/ にはこれまでどおりライトテーマ版だけを置く)。
//
// 実行後、design/screenshots/*.png を DesignSync (write_files + register_assets、
// PNG は @dsCard マーカーを埋め込めないため register_assets が正規の経路) で
// "Boardes UI" プロジェクトへ push する。

#include <QApplication>
#include <QDir>
#include <QImage>
#include <QLibraryInfo>
#include <QLocale>
#include <QPixmap>
#include <QTabWidget>
#include <QTest>
#include <QTranslator>

#include "model/document.h"
#include "model/librarymanager.h"
#include "ui/about.h"
#include "ui/boardeditordialog.h"
#include "ui/duplicatelibrarydialog.h"
#include "ui/exportimageoptionsdialog.h"
#include "ui/exportpackagedialog.h"
#include "ui/input/keymap.h"
#include "ui/input/keymapdialog.h"
#include "ui/librarymanagerdialog.h"
#include "ui/librarymetadatadialog.h"
#include "ui/mainwindow.h"
#include "ui/parteditordialog.h"
#include "ui/partpickerdialog.h"
#include "ui/theme.h"
#include "ui/toolbarcustomizedialog.h"

namespace {

const QString kOutDir = QStringLiteral("/home/tomo/github/Boardes/design/screenshots/");

template <typename Widget>
void shoot(Widget &w, const QString &fileName) {
	w.show();
	QTest::qWaitForWindowExposed(&w);
	const QPixmap pm = w.grab();
	pm.save(kOutDir + fileName);
	w.close();
}

}  // namespace

int main(int argc, char **argv) {
	QApplication app(argc, argv);

	// Phase 20 (claude.ai/design 連携の UI 刷新) のフォント・QSS を適用する。
	// BOARDES_SCREENSHOT_THEME=dark を指定するとダーク版を撮れる (既定はライト)。
	Theme::instance().init();
	if (qEnvironmentVariableIntValue("BOARDES_SCREENSHOT_THEME_DARK") != 0) {
		Theme::instance().forceTheme(true);
	}

	QTranslator qtTranslator;
	if (qtTranslator.load(QLocale(QLocale::Japanese), QStringLiteral("qtbase"), QStringLiteral("_"),
						   QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
		app.installTranslator(&qtTranslator);
	}

	QDir().mkpath(kOutDir);

	// MainWindow / LibraryManagerDialog 等が「自分で loadAll() したときに」実データを
	// 見つけられるよう、同じ AppData 領域に先に PasS ライブラリを取り込んでおく。
	{
		LibraryManager seed;
		seed.loadAll();
		const QString passDir = QStringLiteral("/home/tomo/Documents/pass/parts");
		if (QDir(passDir).exists()) {
			const auto result = seed.importPassFolder(passDir);
			if (!result.ok) {
				qWarning("PasS import failed: %s", qPrintable(result.error));
			}
		}
	}

	// --- 1. メインウィンドウ (オブジェクトタブ。Phase 15 で右ドックの先頭に追加) ---
	{
		MainWindow w;
		w.resize(1500, 950);
		shoot(w, QStringLiteral("main-window-objects.png"));
	}

	// --- 2. メインウィンドウ (DRC タブ) ---
	{
		MainWindow w;
		w.resize(1500, 950);
		w.show();
		QTest::qWaitForWindowExposed(&w);
		if (auto *tabs = w.findChild<QTabWidget *>()) {
			tabs->setCurrentIndex(1);
		}
		const QPixmap pm = w.grab();
		pm.save(kOutDir + QStringLiteral("main-window-drc.png"));
		w.close();
	}

	// --- 3. メインウィンドウ (統計タブ) ---
	{
		MainWindow w;
		w.resize(1500, 950);
		w.show();
		QTest::qWaitForWindowExposed(&w);
		if (auto *tabs = w.findChild<QTabWidget *>()) {
			tabs->setCurrentIndex(2);
		}
		const QPixmap pm = w.grab();
		pm.save(kOutDir + QStringLiteral("main-window-stats.png"));
		w.close();
	}

	// --- 4. About ---
	{
		About w;
		shoot(w, QStringLiteral("about.png"));
	}

	// --- 5. 基板エディタ: パラメトリックモード (画像なし。パッド/銅箔/色を手で指定) ---
	{
		BoardEditorDialog w;
		BoardSpec sample;
		sample.id = QStringLiteral("param-sample");
		sample.name = QStringLiteral("パラメトリック基板サンプル");
		sample.cols = 20;
		sample.rows = 15;
		sample.pitch = 10;
		sample.origin = QPoint(10, 10);
		sample.padShape = PadShape::Round;
		sample.padDiameter = 6;
		sample.holeDiameter = 3;
		sample.copper = CopperPattern::PadPerHole;
		w.setBoard(sample);
		shoot(w, QStringLiteral("board-editor.png"));
	}

	// --- 6. 基板エディタ: 画像モード (実際の PasS 基板 BMP を背景画像として添付) ---
	{
		BoardEditorDialog w;
		BoardSpec sample;
		sample.id = QStringLiteral("icb-504-sample");
		sample.name = QStringLiteral("サンプル基板");
		sample.cols = 53;
		sample.rows = 33;
		sample.pitch = 10;
		sample.origin = QPoint(10, 10);
		const QImage front(QStringLiteral("/home/tomo/Documents/pass/parts/Board/ICB-504.bmp"));
		const QImage back(QStringLiteral("/home/tomo/Documents/pass/parts/Board/ICB-504_.bmp"));
		if (!front.isNull()) {
			sample.backgroundFront = Artwork::fromImageAsIs(front);
		}
		if (!back.isNull()) {
			sample.backgroundBack = Artwork::fromImageAsIs(back);
		}
		w.setBoard(sample);
		shoot(w, QStringLiteral("board-editor-image-mode.png"));
	}

	// --- 7. 部品エディタ (DIP-14 サンプル。基準点は既定の自動 (ピン1)) ---
	{
		PartEditorDialog w;
		Part p;
		p.id = QStringLiteral("IC-14");
		p.name = QStringLiteral("DIP-14 IC");
		p.kind = PartKind::Normal;
		p.refPrefix = QStringLiteral("IC");
		p.keywords = {QStringLiteral("ic"), QStringLiteral("dip")};
		QImage img(70, 30, QImage::Format_RGB888);
		img.fill(QColor(40, 40, 40));
		p.artwork = Artwork::fromImageAsIs(img);
		p.pins = {Pin{1, QPoint(2, 27), 0, {}},  Pin{2, QPoint(12, 27), 0, {}}, Pin{3, QPoint(22, 27), 0, {}},
				  Pin{4, QPoint(32, 27), 0, {}}, Pin{5, QPoint(42, 27), 0, {}}, Pin{6, QPoint(52, 27), 0, {}},
				  Pin{7, QPoint(62, 27), 0, {}}, Pin{8, QPoint(62, 2), 0, {}},  Pin{9, QPoint(52, 2), 0, {}},
				  Pin{10, QPoint(42, 2), 0, {}}, Pin{11, QPoint(32, 2), 0, {}}, Pin{12, QPoint(22, 2), 0, {}},
				  Pin{13, QPoint(12, 2), 0, {}}, Pin{14, QPoint(2, 2), 0, {}}};
		w.setPart(p);
		w.resize(800, 600);
		shoot(w, QStringLiteral("part-editor.png"));
	}

	// --- 8〜10. ライブラリ関連ダイアログ (実際の LibraryManager 状態を使う) ---
	{
		LibraryManager mgr;
		mgr.loadAll();

		{
			LibraryManagerDialog w(&mgr);
			shoot(w, QStringLiteral("library-manager.png"));
		}
		{
			LibraryMetadataDialog w;
			if (const auto lib = mgr.library(LibraryManager::myLibraryId())) {
				w.setLibrary(*lib);
			}
			shoot(w, QStringLiteral("library-metadata.png"));
		}
		{
			if (const auto lib = mgr.library(LibraryManager::passCompatId())) {
				DuplicateLibraryDialog w(*lib);
				shoot(w, QStringLiteral("duplicate-library.png"));
			}
		}
		// --- 11. 部品ピッカー (「他ライブラリから複製」用。新規) ---
		{
			PartPickerDialog w(&mgr, LibraryManager::myLibraryId());
			shoot(w, QStringLiteral("part-picker.png"));
		}
		// --- 12. パッケージエクスポート (依存ライブラリの同梱可否を選ぶ。新規) ---
		{
			ExportPackageDialog w(&mgr, {LibraryManager::passCompatId(), LibraryManager::myLibraryId()});
			shoot(w, QStringLiteral("export-package.png"));
		}
	}

	// --- 13. 画像エクスポート設定 ---
	{
		ExportImageOptionsDialog w;
		shoot(w, QStringLiteral("export-image-options.png"));
	}

	// --- 14. 操作のカスタマイズ (ショートカット/マウス割り当て。新規) ---
	{
		Keymap keymap;
		keymap.load();
		KeymapDialog w(&keymap);
		shoot(w, QStringLiteral("keymap-settings.png"));
	}

	// --- 15. ツールバーのカスタマイズ (LibreOffice 風。新規) ---
	{
		MainWindow mw;  // 実際に構築されたツールバー・ActionRegistry を使うため
		ToolbarCustomizeDialog w(&mw.actionRegistry());
		shoot(w, QStringLiteral("toolbar-customize.png"));
	}

	return 0;
}
