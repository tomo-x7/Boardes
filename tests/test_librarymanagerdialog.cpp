#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>

#include "model/librarymanager.h"
#include "ui/librarymanagerdialog.h"

// ボタン群・詳細ラベルはダイアログの private メンバなので、テストからは
// findChild<QPushButton*>(objectName) 等の名前引きはできない (オブジェクト名を
// 振っていないため)。その代わり、ボタンのテキストで区別する。
class TestLibraryManagerDialog : public QObject {
	Q_OBJECT

private slots:
	void init();
	void cleanup();

	void listShowsAllLibrariesAndAutoSelectsOne();
	void myLibrarySelectionEnablesEditButNotExportOrRemove();
	void passCompatSelectionEnablesDuplicateOnlyAmongMutators();
	void editableDuplicateEnablesExportAndRemove();
	void listRefreshesWhenLibraryManagerChangesExternally();

private:
	std::unique_ptr<QTemporaryDir> m_appDataDir;
	std::unique_ptr<LibraryManager> m_libMgr;

	static QPushButton *buttonWithText(const LibraryManagerDialog &dialog, const QString &text);
	void selectLibrary(LibraryManagerDialog &dialog, const QString &libraryId);
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
		if (b->text().contains(text)) {
			return b;
		}
	}
	return nullptr;
}

void TestLibraryManagerDialog::selectLibrary(LibraryManagerDialog &dialog, const QString &libraryId) {
	auto *list = dialog.findChild<QListWidget *>();
	QVERIFY(list != nullptr);
	for (int i = 0; i < list->count(); ++i) {
		if (list->item(i)->data(Qt::UserRole).toString() == libraryId) {
			list->setCurrentRow(i);
			return;
		}
	}
	QFAIL("library not found in list");
}

void TestLibraryManagerDialog::listShowsAllLibrariesAndAutoSelectsOne() {
	LibraryManagerDialog dialog(m_libMgr.get());
	auto *list = dialog.findChild<QListWidget *>();
	QVERIFY(list != nullptr);
	QCOMPARE(list->count(), 2);  // マイライブラリ + PasS互換
	QVERIFY(list->currentItem() != nullptr);

	auto *details = dialog.findChild<QLabel *>();
	QVERIFY(details != nullptr);
	QVERIFY(!details->text().contains(QStringLiteral("選択されていません")));
}

void TestLibraryManagerDialog::myLibrarySelectionEnablesEditButNotExportOrRemove() {
	LibraryManagerDialog dialog(m_libMgr.get());
	selectLibrary(dialog, LibraryManager::myLibraryId());

	auto *details = dialog.findChild<QLabel *>();
	QVERIFY(details->text().contains(QStringLiteral("編集: 可")));

	QVERIFY(buttonWithText(dialog, QStringLiteral("複製"))->isEnabled());
	QVERIFY(buttonWithText(dialog, QStringLiteral("メタデータ"))->isEnabled());
	QVERIFY(!buttonWithText(dialog, QStringLiteral("エクスポート"))->isEnabled());
	QVERIFY(!buttonWithText(dialog, QStringLiteral("削除"))->isEnabled());
}

void TestLibraryManagerDialog::passCompatSelectionEnablesDuplicateOnlyAmongMutators() {
	LibraryManagerDialog dialog(m_libMgr.get());
	selectLibrary(dialog, LibraryManager::passCompatId());

	auto *details = dialog.findChild<QLabel *>();
	QVERIFY(details->text().contains(QStringLiteral("編集: 不可")));

	QVERIFY(buttonWithText(dialog, QStringLiteral("複製"))->isEnabled());
	QVERIFY(!buttonWithText(dialog, QStringLiteral("メタデータ"))->isEnabled());  // readOnly
	QVERIFY(!buttonWithText(dialog, QStringLiteral("エクスポート"))->isEnabled());  // 再配布不可
	QVERIFY(!buttonWithText(dialog, QStringLiteral("削除"))->isEnabled());          // 組み込み
}

void TestLibraryManagerDialog::editableDuplicateEnablesExportAndRemove() {
	LibraryManager::DuplicateSpec spec;
	spec.newId = QStringLiteral("editable-dup");
	spec.newName = QStringLiteral("複製されたライブラリ");
	spec.newAuthor = QStringLiteral("tomo-x");
	spec.newVersion = QStringLiteral("1.0.0");
	spec.newLicense.kind = LicenseKind::CC0_1_0;
	QVERIFY(m_libMgr->duplicateLibrary(LibraryManager::passCompatId(), spec).ok);

	LibraryManagerDialog dialog(m_libMgr.get());
	selectLibrary(dialog, QStringLiteral("editable-dup"));

	QVERIFY(buttonWithText(dialog, QStringLiteral("複製"))->isEnabled());
	QVERIFY(buttonWithText(dialog, QStringLiteral("メタデータ"))->isEnabled());
	QVERIFY(buttonWithText(dialog, QStringLiteral("エクスポート"))->isEnabled());
	QVERIFY(buttonWithText(dialog, QStringLiteral("削除"))->isEnabled());

	auto *details = dialog.findChild<QLabel *>();
	QVERIFY(details->text().contains(QStringLiteral("複製元:")));
}

void TestLibraryManagerDialog::listRefreshesWhenLibraryManagerChangesExternally() {
	LibraryManagerDialog dialog(m_libMgr.get());
	auto *list = dialog.findChild<QListWidget *>();
	QCOMPARE(list->count(), 2);

	// ダイアログの外 (例: メニューからの直接インポート) でライブラリが増えても、
	// librariesChanged シグナル経由で一覧が追従すること。
	LibraryManager::DuplicateSpec spec;
	spec.newId = QStringLiteral("external-add");
	spec.newName = QStringLiteral("外部から追加");
	spec.newAuthor = QStringLiteral("tomo-x");
	spec.newVersion = QStringLiteral("1.0.0");
	spec.newLicense.kind = LicenseKind::MIT;
	QVERIFY(m_libMgr->duplicateLibrary(LibraryManager::myLibraryId(), spec).ok);

	QCOMPARE(list->count(), 3);
}

QTEST_MAIN(TestLibraryManagerDialog)
#include "test_librarymanagerdialog.moc"
