#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include "io/documentio.h"
#include "io/libraryio.h"
#include "io/loadresult.h"
#include "io/zipio.h"
#include "model/document.h"

// Phase 17: zip エントリ名に ".." を含む (zip slip) アーカイブを、.blib / .bpkg の
// どちらの取り込み経路でも拒否することを確認する。
class TestZipValidation : public QObject {
	Q_OBJECT

private slots:
	void importFromBlibRejectsPathTraversalEntry();
	void importPackageRejectsPathTraversalEntry();
};

void TestZipValidation::importFromBlibRejectsPathTraversalEntry() {
	ZipWriter writer;
	QJsonObject lib;
	lib["schema"] = QStringLiteral("boardes.library/1");
	lib["id"] = QStringLiteral("evil-lib");
	lib["name"] = QStringLiteral("悪意のあるライブラリ");
	QVERIFY(writer.addFile(QStringLiteral("library.json"), QJsonDocument(lib).toJson()));
	// 上位ディレクトリへ抜け出そうとするエントリ名 (zip slip)。
	QVERIFY(writer.addFile(QStringLiteral("../../evil.txt"), QByteArray("gotcha")));
	const QByteArray zipBytes = writer.finish();
	QVERIFY(writer.isValid());
	QVERIFY(!zipBytes.isEmpty());

	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	const QString blibPath = dir.filePath("evil.blib");
	QFile f(blibPath);
	QVERIFY(f.open(QIODevice::WriteOnly));
	QCOMPARE(f.write(zipBytes), static_cast<qint64>(zipBytes.size()));
	f.close();

	LoadResult result;
	const auto loaded = libraryio::importFromBlib(blibPath, &result);
	QVERIFY(!loaded.has_value());
	QVERIFY(!result.ok);
	QVERIFY(!result.detail.isEmpty());
}

void TestZipValidation::importPackageRejectsPathTraversalEntry() {
	ZipWriter writer;
	Document doc;
	doc.title = QStringLiteral("正常なドキュメント");
	QVERIFY(writer.addFile(QStringLiteral("document.boardes"),
							QJsonDocument(documentio::toJsonObject(doc)).toJson()));
	// 相対パスで上位ディレクトリへ抜け出そうとするエントリ名 (zip slip)。
	QVERIFY(writer.addFile(QStringLiteral("../evil.txt"), QByteArray("gotcha")));
	const QByteArray zipBytes = writer.finish();
	QVERIFY(writer.isValid());
	QVERIFY(!zipBytes.isEmpty());

	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	const QString bpkgPath = dir.filePath("evil.bpkg");
	QFile f(bpkgPath);
	QVERIFY(f.open(QIODevice::WriteOnly));
	QCOMPARE(f.write(zipBytes), static_cast<qint64>(zipBytes.size()));
	f.close();

	Document loaded;
	const auto result = documentio::importPackage(bpkgPath, loaded, nullptr);
	QVERIFY(!result.ok);
	QVERIFY(!result.error.isEmpty());
}

QTEST_MAIN(TestZipValidation)
#include "test_zipvalidation.moc"
