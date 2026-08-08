#include <QLabel>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

#include "model/librarymanager.h"
#include "ui/librarymanagerdialog.h"

// ボタン群・詳細ラベルはダイアログの private メンバなので、テストからは
// findChild<QPushButton*>(objectName) 等の名前引きはできない (オブジェクト名を
// 振っていないため)。その代わり、ボタンのテキストの完全一致で区別する
// (部分一致だと「編集...」/「メタデータ編集...」のように互いを部分文字列として
// 含んでしまうボタンが複数あるため)。
class TestLibraryManagerDialog : public QObject {
	Q_OBJECT

private slots:
	void init();
	void cleanup();

	void treeShowsLibrariesWithCategoryAndBoardRoots();
	void librarySelectionEnablesLibraryOperations();
	void categoryRootSelectionOnlyEnablesAddButtons();
	void partSelectionEnablesItemOperationsButNotLibraryOnes();
	void uncategorizedCategoryCannotBeDeleted();
	void treeRefreshesWhenLibraryManagerChangesExternally();

private:
	std::unique_ptr<QTemporaryDir> m_appDataDir;
	std::unique_ptr<LibraryManager> m_libMgr;

	static QPushButton *buttonWithText(const LibraryManagerDialog &dialog, const QString &text);
	static QTreeWidgetItem *findItem(QTreeWidget *tree, LibraryManagerDialog::NodeType type, const QString &libId,
									 const QString &itemId = QString());
};

void TestLibraryManagerDialog::init() {
	m_appDataDir = std::make_unique<QTemporaryDir>();
	QVERIFY(m_appDataDir->isValid());
	qputenv("XDG_DATA_HOME", m_appDataDir->path().toUtf8());
	m_libMgr = std::make_unique<LibraryManager>();
	m_libMgr->loadAll();
}

void TestLibraryManagerDialog::cleanup() {
	m_libMgr.reset();
	m_appDataDir.reset();
}

QPushButton *TestLibraryManagerDialog::buttonWithText(const LibraryManagerDialog &dialog, const QString &text) {
	for (QPushButton *b : dialog.findChildren<QPushButton *>()) {
		if (b->text() == text) {
			return b;
		}
	}
	return nullptr;
}

QTreeWidgetItem *TestLibraryManagerDialog::findItem(QTreeWidget *tree, LibraryManagerDialog::NodeType type,
													const QString &libId, const QString &itemId) {
	QTreeWidgetItemIterator it(tree);
	while (*it) {
		if (static_cast<LibraryManagerDialog::NodeType>((*it)->data(0, Qt::UserRole).toInt()) == type &&
			(*it)->data(0, Qt::UserRole + 1).toString() == libId &&
			(itemId.isEmpty() || (*it)->data(0, Qt::UserRole + 2).toString() == itemId)) {
			return *it;
		}
		++it;
	}
	return nullptr;
}

void TestLibraryManagerDialog::treeShowsLibrariesWithCategoryAndBoardRoots() {
	LibraryManagerDialog dialog(m_libMgr.get());
	auto *tree = dialog.findChild<QTreeWidget *>();
	QVERIFY(tree != nullptr);
	QCOMPARE(tree->topLevelItemCount(), 2);  // マイライブラリ + PasS互換

	QVERIFY(findItem(tree, LibraryManagerDialog::NodeType::Library, LibraryManager::myLibraryId()) != nullptr);
	QVERIFY(findItem(tree, LibraryManagerDialog::NodeType::CategoryRoot, LibraryManager::myLibraryId()) != nullptr);
	QVERIFY(findItem(tree, LibraryManagerDialog::NodeType::BoardRoot, LibraryManager::myLibraryId()) != nullptr);
	QVERIFY(findItem(tree, LibraryManagerDialog::NodeType::Category, LibraryManager::myLibraryId(),
					 LibraryManager::uncategorizedCategoryId()) != nullptr);
}

void TestLibraryManagerDialog::librarySelectionEnablesLibraryOperations() {
	LibraryManagerDialog dialog(m_libMgr.get());
	auto *tree = dialog.findChild<QTreeWidget *>();
	tree->setCurrentItem(findItem(tree, LibraryManagerDialog::NodeType::Library, LibraryManager::myLibraryId()));

	QVERIFY(buttonWithText(dialog, QStringLiteral("複製..."))->isEnabled());
	QVERIFY(buttonWithText(dialog, QStringLiteral("メタデータ編集..."))->isEnabled());
	QVERIFY(buttonWithText(dialog, QStringLiteral("エクスポート (.blib)..."))->isEnabled());
	// Phase 14: readOnly 廃止によりビルトインライブラリも削除ボタンが有効。
	QVERIFY(buttonWithText(dialog, QStringLiteral("ライブラリを削除"))->isEnabled());
	QVERIFY(buttonWithText(dialog, QStringLiteral("部品を追加..."))->isEnabled());
	QVERIFY(buttonWithText(dialog, QStringLiteral("基板を追加..."))->isEnabled());

	auto *details = dialog.findChild<QLabel *>();
	QVERIFY(details->text().contains(QStringLiteral("再配布: 不可")));
}

void TestLibraryManagerDialog::categoryRootSelectionOnlyEnablesAddButtons() {
	LibraryManagerDialog dialog(m_libMgr.get());
	auto *tree = dialog.findChild<QTreeWidget *>();
	tree->setCurrentItem(findItem(tree, LibraryManagerDialog::NodeType::CategoryRoot, LibraryManager::myLibraryId()));

	// CategoryRoot はライブラリ配下なので追加系は使えるが、ライブラリ自体の操作は使えない。
	QVERIFY(buttonWithText(dialog, QStringLiteral("部品を追加..."))->isEnabled());
	QVERIFY(!buttonWithText(dialog, QStringLiteral("複製..."))->isEnabled());
	QVERIFY(!buttonWithText(dialog, QStringLiteral("メタデータ編集..."))->isEnabled());
	QVERIFY(!buttonWithText(dialog, QStringLiteral("エクスポート (.blib)..."))->isEnabled());
}

void TestLibraryManagerDialog::partSelectionEnablesItemOperationsButNotLibraryOnes() {
	Part part;
	part.id = QStringLiteral("P1");
	part.name = QStringLiteral("テスト部品");
	QVERIFY(m_libMgr->addPartToMyLibrary(part).ok);

	LibraryManagerDialog dialog(m_libMgr.get());
	auto *tree = dialog.findChild<QTreeWidget *>();
	auto *item = findItem(tree, LibraryManagerDialog::NodeType::Part, LibraryManager::myLibraryId(), "P1");
	QVERIFY(item != nullptr);
	tree->setCurrentItem(item);

	QVERIFY(buttonWithText(dialog, QStringLiteral("編集..."))->isEnabled());
	QVERIFY(buttonWithText(dialog, QStringLiteral("選択項目を削除"))->isEnabled());
	QVERIFY(buttonWithText(dialog, QStringLiteral("カテゴリを変更..."))->isEnabled());
	QVERIFY(buttonWithText(dialog, QStringLiteral("他ライブラリから複製..."))->isEnabled());
	QVERIFY(!buttonWithText(dialog, QStringLiteral("複製..."))->isEnabled());
	QVERIFY(!buttonWithText(dialog, QStringLiteral("ライブラリを削除"))->isEnabled());

	auto *details = dialog.findChild<QLabel *>();
	QVERIFY(details->text().contains(QStringLiteral("テスト部品")));
}

void TestLibraryManagerDialog::uncategorizedCategoryCannotBeDeleted() {
	LibraryManagerDialog dialog(m_libMgr.get());
	auto *tree = dialog.findChild<QTreeWidget *>();
	auto *item = findItem(tree, LibraryManagerDialog::NodeType::Category, LibraryManager::myLibraryId(),
						  LibraryManager::uncategorizedCategoryId());
	QVERIFY(item != nullptr);
	tree->setCurrentItem(item);

	QVERIFY(buttonWithText(dialog, QStringLiteral("名前変更..."))->isEnabled());
	QVERIFY(!buttonWithText(dialog, QStringLiteral("カテゴリを削除"))->isEnabled());
	QVERIFY(!buttonWithText(dialog, QStringLiteral("ライブラリを削除"))->isEnabled());
	QVERIFY(!buttonWithText(dialog, QStringLiteral("選択項目を削除"))->isEnabled());
}

void TestLibraryManagerDialog::treeRefreshesWhenLibraryManagerChangesExternally() {
	LibraryManagerDialog dialog(m_libMgr.get());
	auto *tree = dialog.findChild<QTreeWidget *>();
	QCOMPARE(tree->topLevelItemCount(), 2);

	// ダイアログの外 (例: 別経路からの直接インポート) でライブラリが増えても、
	// librariesChanged シグナル経由で一覧が追従すること。
	Library lib;
	lib.id = QStringLiteral("external-add");
	lib.name = QStringLiteral("外部から追加");
	QVERIFY(m_libMgr->createLibrary(lib).ok);

	QCOMPARE(tree->topLevelItemCount(), 3);
}

QTEST_MAIN(TestLibraryManagerDialog)
#include "test_librarymanagerdialog.moc"
