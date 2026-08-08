#include <QCoreApplication>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "ui/input/commandregistry.h"
#include "ui/input/keymap.h"

// Phase 18: Keymap (ショートカット/マウス割り当てのカスタマイズ) のユニットテスト。
class TestKeymap : public QObject {
	Q_OBJECT

private slots:
	void initTestCase();
	void init();

	void gesturesForReturnsDefaultsWhenNotCustomized();
	void setGesturesOverridesDefaultsAndPersists();
	void resetRestoresDefault();
	void resetAllClearsEveryOverride();
	void loadIgnoresIndividuallyBrokenStoredGestures();
	void conflictsDetectSameCategoryDuplicateAssignment();
	void conflictsIgnoreDifferentCategoriesWithoutGlobal();
	void displayForJoinsMultipleGesturesWithOr();
	void changedSignalFiresOnMutation();

private:
	std::unique_ptr<QTemporaryDir> m_configDir;
};

void TestKeymap::initTestCase() {
	// main.cpp と衝突しない専用の識別子にして、実ユーザーの設定ファイルを汚さない。
	QCoreApplication::setOrganizationName(QStringLiteral("tomo-x"));
	QCoreApplication::setApplicationName(QStringLiteral("Boardes-Test"));
}

void TestKeymap::init() {
	m_configDir = std::make_unique<QTemporaryDir>();
	QVERIFY(m_configDir->isValid());
	qputenv("XDG_CONFIG_HOME", m_configDir->path().toUtf8());
}

void TestKeymap::gesturesForReturnsDefaultsWhenNotCustomized() {
	Keymap keymap;
	keymap.load();
	const auto *def = commandregistry::find(QStringLiteral("select.rotate"));
	QVERIFY(def != nullptr);
	QCOMPARE(keymap.gesturesFor(QStringLiteral("select.rotate")), def->defaults);
	QVERIFY(!keymap.isCustomized(QStringLiteral("select.rotate")));
}

void TestKeymap::setGesturesOverridesDefaultsAndPersists() {
	InputGesture g;
	g.kind = InputKind::Key;
	g.key = Qt::Key_T;
	g.mods = Qt::NoModifier;

	{
		Keymap keymap;
		keymap.load();
		keymap.setGestures(QStringLiteral("select.rotate"), {g});
		QVERIFY(keymap.isCustomized(QStringLiteral("select.rotate")));
		QCOMPARE(keymap.gesturesFor(QStringLiteral("select.rotate")).size(), 1);
	}
	// 別インスタンスで読み直しても保持されていること (QSettings 経由の永続化)。
	Keymap reloaded;
	reloaded.load();
	const auto gestures = reloaded.gesturesFor(QStringLiteral("select.rotate"));
	QCOMPARE(gestures.size(), 1);
	QCOMPARE(gestures.first(), g);
}

void TestKeymap::resetRestoresDefault() {
	Keymap keymap;
	keymap.load();
	InputGesture g;
	g.kind = InputKind::Key;
	g.key = Qt::Key_T;
	keymap.setGestures(QStringLiteral("select.rotate"), {g});
	QVERIFY(keymap.isCustomized(QStringLiteral("select.rotate")));

	keymap.reset(QStringLiteral("select.rotate"));
	QVERIFY(!keymap.isCustomized(QStringLiteral("select.rotate")));
	const auto *def = commandregistry::find(QStringLiteral("select.rotate"));
	QCOMPARE(keymap.gesturesFor(QStringLiteral("select.rotate")), def->defaults);
}

void TestKeymap::resetAllClearsEveryOverride() {
	Keymap keymap;
	keymap.load();
	InputGesture g;
	g.kind = InputKind::Key;
	g.key = Qt::Key_T;
	keymap.setGestures(QStringLiteral("select.rotate"), {g});
	keymap.setGestures(QStringLiteral("select.flip"), {g});
	QCOMPARE(keymap.customizedCommandIds().size(), 2);

	keymap.resetAll();
	QVERIFY(keymap.customizedCommandIds().isEmpty());

	Keymap reloaded;
	reloaded.load();
	QVERIFY(reloaded.customizedCommandIds().isEmpty());
}

void TestKeymap::loadIgnoresIndividuallyBrokenStoredGestures() {
	{
		Keymap keymap;
		keymap.load();
		InputGesture valid;
		valid.kind = InputKind::Key;
		valid.key = Qt::Key_T;
		keymap.setGestures(QStringLiteral("select.rotate"), {valid});
	}
	// QSettings に直接、壊れた文字列を紛れ込ませる (壊れた設定は個別に無視して既定へ
	// フォールバックする、という Phase 17 の設定バリデーション方針を Keymap にも適用する)。
	{
		QSettings settings;
		settings.beginGroup(QStringLiteral("keymap"));
		settings.setValue(QStringLiteral("select.flip"), QStringList{QStringLiteral("not-a-valid-gesture")});
		settings.endGroup();
	}
	Keymap reloaded;
	reloaded.load();
	// select.rotate は正常に読み込まれている。
	QVERIFY(reloaded.isCustomized(QStringLiteral("select.rotate")));
	// select.flip は全滅したので上書きなし = 既定値にフォールバックする。
	QVERIFY(!reloaded.isCustomized(QStringLiteral("select.flip")));
	const auto *def = commandregistry::find(QStringLiteral("select.flip"));
	QCOMPARE(reloaded.gesturesFor(QStringLiteral("select.flip")), def->defaults);
}

void TestKeymap::conflictsDetectSameCategoryDuplicateAssignment() {
	Keymap keymap;
	keymap.load();
	InputGesture g;
	g.kind = InputKind::Key;
	g.key = Qt::Key_Delete;
	// select.flip を select.delete の既定 (Key_Delete) と衝突させる。
	keymap.setGestures(QStringLiteral("select.flip"), {g});

	const auto conflicts = keymap.conflicts();
	bool found = false;
	for (const auto &c : conflicts) {
		if ((c.commandIdA == QStringLiteral("select.delete") && c.commandIdB == QStringLiteral("select.flip")) ||
			(c.commandIdB == QStringLiteral("select.delete") && c.commandIdA == QStringLiteral("select.flip"))) {
			found = true;
		}
	}
	QVERIFY(found);
}

void TestKeymap::conflictsIgnoreDifferentCategoriesWithoutGlobal() {
	Keymap keymap;
	keymap.load();
	InputGesture g;
	g.kind = InputKind::Key;
	g.key = Qt::Key_Z;
	// select と draft は別カテゴリで、どちらも global ではない。
	keymap.setGestures(QStringLiteral("select.flip"), {g});
	keymap.setGestures(QStringLiteral("place.rotate"), {g});  // select とも place とも重ならない別カテゴリの例として使う

	const auto conflicts = keymap.conflicts();
	for (const auto &c : conflicts) {
		const bool involvesFlipAndRotate =
			(c.commandIdA == QStringLiteral("select.flip") && c.commandIdB == QStringLiteral("place.rotate")) ||
			(c.commandIdB == QStringLiteral("select.flip") && c.commandIdA == QStringLiteral("place.rotate"));
		QVERIFY(!involvesFlipAndRotate);  // select と place は無関係のカテゴリなので衝突扱いしない
	}
}

void TestKeymap::displayForJoinsMultipleGesturesWithOr() {
	Keymap keymap;
	keymap.load();
	const QString display = keymap.displayFor(QStringLiteral("wire.commit"));
	// 既定値は右クリック / Return / Enter の3つ。少なくとも複数個を含む表示になること。
	QVERIFY(display.contains(QStringLiteral("または")));
}

void TestKeymap::changedSignalFiresOnMutation() {
	Keymap keymap;
	keymap.load();
	QSignalSpy spy(&keymap, &Keymap::changed);
	InputGesture g;
	g.kind = InputKind::Key;
	g.key = Qt::Key_T;
	keymap.setGestures(QStringLiteral("select.rotate"), {g});
	QCOMPARE(spy.count(), 1);
	keymap.reset(QStringLiteral("select.rotate"));
	QCOMPARE(spy.count(), 2);
	keymap.resetAll();
	QCOMPARE(spy.count(), 3);
}

QTEST_MAIN(TestKeymap)
#include "test_keymap.moc"
