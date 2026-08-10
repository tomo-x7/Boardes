#include <QApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>

#include "ui/mainwindow.h"
#include "ui/theme.h"

int main(int argc, char *argv[]) {
	QCoreApplication::setOrganizationName("tomo-x");
	QCoreApplication::setOrganizationDomain("tomo-x.win");
	QCoreApplication::setApplicationName("Boardes");

	QApplication app(argc, argv);

	// フォント埋め込み・カラーテーマ (ライト/ダーク) の適用。Phase 20 (claude.ai/design 連携の
	// UI 刷新)。QApplication 生成直後、他のウィジェットを作る前に呼ぶ必要がある。
	Theme::instance().init();

	// アプリ自体の文言はすべて日本語で書いているが、QDialogButtonBox の標準ボタン
	// (OK/Cancel/Close 等) は Qt 本体の翻訳カタログ (qtbase) 任せになるため、
	// これを読み込まないとそこだけ英語のまま残ってしまう。無ければ何もしない
	// (英語表示にフォールバックするだけで、動作自体に支障は無い)。
	auto *qtTranslator = new QTranslator(&app);
	if (qtTranslator->load(QLocale(QLocale::Japanese), QStringLiteral("qtbase"), QStringLiteral("_"),
							QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
		app.installTranslator(qtTranslator);
	}

	MainWindow window;
	window.show();
	return app.exec();
}
