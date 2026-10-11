#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "model/librarymanager.h"
#include "ui/boardmenu.h"

// メインウィンドウの「基板」メニュー。ライブラリ管理を開かなくても基板を選べることと、
// 一覧の作り方 (ライブラリの数による階層の有無・並び順・現在の基板のチェック) を確かめる。
class TestBoardMenu : public QObject {
	Q_OBJECT

private slots:
	void init();
	void cleanup();
	void showsPlaceholderWhenNoLibraryHasBoards();
	void listsBoardsOfSingleLibraryWithoutSubmenu();
	void groupsBoardsBySubmenuWhenSeveralLibrariesHaveBoards();
	void choosingABoardEmitsIt();
	void checksTheCurrentBoardAndEnablesEditing();
	void buttonTextShowsNameAndGrid();

private:
	std::unique_ptr<QTemporaryDir> m_appDataDir;
	std::unique_ptr<LibraryManager> m_libMgr;

	static BoardSpec makeBoard(const QString &id, const QString &name, int cols, int rows);
	// 区切り線とサブメニューを除いた、基板を表す項目 (チェック可能な項目) の文字列。
	static QStringList boardTexts(const QMenu *menu);
	static QAction *actionStartingWith(const QMenu *menu, const QString &prefix);
};

BoardSpec TestBoardMenu::makeBoard(const QString &id, const QString &name, int cols, int rows) {
	BoardSpec b;
	b.id = id;
	b.name = name;
	b.cols = cols;
	b.rows = rows;
	b.pitch = 10;
	b.origin = QPoint(10, 10);
	b.size = QSize(2 * 10 + (cols - 1) * 10, 2 * 10 + (rows - 1) * 10);
	return b;
}

QStringList TestBoardMenu::boardTexts(const QMenu *menu) {
	QStringList texts;
	for (QAction *a : menu->actions()) {
		if (a->isCheckable()) {
			texts << a->text();
		}
	}
	return texts;
}

QAction *TestBoardMenu::actionStartingWith(const QMenu *menu, const QString &prefix) {
	for (QAction *a : menu->actions()) {
		if (a->text().startsWith(prefix)) {
			return a;
		}
	}
	return nullptr;
}

void TestBoardMenu::init() {
	m_appDataDir = std::make_unique<QTemporaryDir>();
	QVERIFY(m_appDataDir->isValid());
	qputenv("XDG_DATA_HOME", m_appDataDir->path().toUtf8());  // 実ユーザー環境を汚さない
	m_libMgr = std::make_unique<LibraryManager>();
	m_libMgr->loadAll();
}

void TestBoardMenu::cleanup() {
	m_libMgr.reset();
	m_appDataDir.reset();
}

void TestBoardMenu::showsPlaceholderWhenNoLibraryHasBoards() {
	BoardMenu menu(m_libMgr.get());
	QAction *placeholder = actionStartingWith(&menu, "ライブラリに基板がありません");
	QVERIFY(placeholder != nullptr);
	QVERIFY(!placeholder->isEnabled());
	// 新規作成はいつでもできる。現在の基板が無いので、編集・登録はできない。
	QVERIFY(actionStartingWith(&menu, "新規基板")->isEnabled());
	QVERIFY(!actionStartingWith(&menu, "現在の基板を編集")->isEnabled());
	QVERIFY(!actionStartingWith(&menu, "現在の基板をマイライブラリに登録")->isEnabled());
}

void TestBoardMenu::listsBoardsOfSingleLibraryWithoutSubmenu() {
	QVERIFY(m_libMgr->addBoardToMyLibrary(makeBoard("a", "Beta", 30, 20)).ok);
	QVERIFY(m_libMgr->addBoardToMyLibrary(makeBoard("b", "Alpha", 72, 47)).ok);
	BoardMenu menu(m_libMgr.get());
	// 階層を挟まずに名前順に並び (id 順ではない)、格子の大きさをタブの後ろに出す。
	QCOMPARE(boardTexts(&menu), (QStringList{"Alpha\t72×47", "Beta\t30×20"}));
	QVERIFY(menu.findChildren<QMenu *>(Qt::FindDirectChildrenOnly).isEmpty());
}

void TestBoardMenu::groupsBoardsBySubmenuWhenSeveralLibrariesHaveBoards() {
	QVERIFY(m_libMgr->addBoardToMyLibrary(makeBoard("mine", "Mine", 10, 10)).ok);
	QVERIFY(m_libMgr->addBoardTo(LibraryManager::passCompatId(), makeBoard("pass", "PasS Board", 20, 20)).ok);
	BoardMenu menu(m_libMgr.get());
	const auto submenus = menu.findChildren<QMenu *>(Qt::FindDirectChildrenOnly);
	QCOMPARE(submenus.size(), 2);
	QStringList all;
	for (const QMenu *sub : submenus) {
		all << boardTexts(sub);
	}
	all.sort();
	QCOMPARE(all, (QStringList{"Mine\t10×10", "PasS Board\t20×20"}));

	// 開き直す (rebuild) たびにサブメニューを作り直しても、古いものが残らない。
	menu.rebuild();
	menu.rebuild();
	QCOMPARE(menu.findChildren<QMenu *>(Qt::FindDirectChildrenOnly).size(), 2);
}

void TestBoardMenu::choosingABoardEmitsIt() {
	QVERIFY(m_libMgr->addBoardToMyLibrary(makeBoard("icb", "ICB-93", 26, 36)).ok);
	BoardMenu menu(m_libMgr.get());
	QSignalSpy spy(&menu, &BoardMenu::boardChosen);
	actionStartingWith(&menu, "ICB-93")->trigger();
	QCOMPARE(spy.count(), 1);
	const auto board = spy.first().first().value<BoardSpec>();
	QCOMPARE(board.id, QString("icb"));
	QCOMPARE(board.cols, 26);
	QCOMPARE(board.rows, 36);
}

void TestBoardMenu::checksTheCurrentBoardAndEnablesEditing() {
	const BoardSpec a = makeBoard("a", "A", 10, 10);
	const BoardSpec b = makeBoard("b", "B", 20, 20);
	QVERIFY(m_libMgr->addBoardToMyLibrary(a).ok);
	QVERIFY(m_libMgr->addBoardToMyLibrary(b).ok);
	BoardMenu menu(m_libMgr.get());
	const BoardSpec current = b;  // 設計データへコピーされた基板
	menu.setCurrentBoard(&current);
	menu.rebuild();
	QVERIFY(!actionStartingWith(&menu, "A\t")->isChecked());
	QVERIFY(actionStartingWith(&menu, "B\t")->isChecked());
	QVERIFY(actionStartingWith(&menu, "現在の基板を編集")->isEnabled());
	QVERIFY(actionStartingWith(&menu, "現在の基板をマイライブラリに登録")->isEnabled());

	// 同じ id でも、編集して格子が変わった基板は「ライブラリの基板そのもの」ではない。
	BoardSpec edited = b;
	edited.cols = 21;
	menu.setCurrentBoard(&edited);
	menu.rebuild();
	QVERIFY(!actionStartingWith(&menu, "B\t")->isChecked());
}

void TestBoardMenu::buttonTextShowsNameAndGrid() {
	QCOMPARE(BoardMenu::buttonText(nullptr), QString("基板: 未設定"));
	const BoardSpec empty;  // 新規の設計データの基板 (サイズが空)
	QCOMPARE(BoardMenu::buttonText(&empty), QString("基板: 未設定"));
	const BoardSpec board = makeBoard("icb", "ICB-93", 26, 36);
	QCOMPARE(BoardMenu::buttonText(&board), QString("基板: ICB-93 (26×36)"));
	BoardSpec unnamed = board;
	unnamed.name.clear();
	QCOMPARE(BoardMenu::buttonText(&unnamed), QString("基板: icb (26×36)"));
	// ボタンの文字列では & がアクセラレータになるので、そのまま見えるよう && にする。
	BoardSpec ampersand = board;
	ampersand.name = "A&B";
	QCOMPARE(BoardMenu::buttonText(&ampersand), QString("基板: A&&B (26×36)"));
}

QTEST_MAIN(TestBoardMenu)
#include "test_boardmenu.moc"
