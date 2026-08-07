#include <QApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>

#include "ui/mainwindow.h"

int main(int argc, char *argv[]) {
	QCoreApplication::setOrganizationName("tomo-x");
	QCoreApplication::setOrganizationDomain("tomo-x.win");
	QCoreApplication::setApplicationName("Boardes");

	QApplication app(argc, argv);

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
