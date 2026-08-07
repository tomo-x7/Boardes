#pragma once

#include <QMap>
#include <QObject>
#include <QString>
#include <memory>
#include <optional>

#include "library.h"

// 実行時に読み込まれている全ライブラリを管理するレジストリ。
//
// 常に以下が存在する:
//   - マイライブラリ (id: myLibraryId())    編集可 / export 不可 (常に)
//   - PasS互換       (id: passCompatId())   編集不可 / export 不可 (常に)
// それに加えて、.blib からインストールしたライブラリや、複製で作った
// 編集可能なライブラリが増えていく。
//
// 保存先は QStandardPaths::AppDataLocation 配下の "libraries/<libId>/" ディレクトリ
// (ディレクトリ展開形の library.json 一式)。
class LibraryManager : public QObject {
	Q_OBJECT

public:
	explicit LibraryManager(QObject *parent = nullptr);

	static QString myLibraryId() {
		return QStringLiteral("my-library");
	}
	static QString passCompatId() {
		return QStringLiteral("pass-compat");
	}
	static QString uncategorizedCategoryId() {
		return QStringLiteral("uncategorized");
	}

	// AppDataLocation/libraries/ 以下の全ライブラリを読み込み、無ければマイライブラリ・
	// PasS互換 (空) を新規作成する。アプリ起動時に一度呼ぶ。
	void loadAll();

	QString storageDir() const;

	const QMap<QString, std::shared_ptr<Library>> &libraries() const {
		return m_libraries;
	}
	std::shared_ptr<Library> library(const QString &id) const {
		return m_libraries.value(id);
	}
	std::shared_ptr<Part> resolvePart(const QString &libraryId, const QString &partId) const;
	std::shared_ptr<BoardSpec> resolveBoard(const QString &libraryId, const QString &boardId) const;

	struct OpResult {
		bool ok = false;
		QString error;
		QString libraryId;
		int partCount = 0;
		int boardCount = 0;
		int categoryCount = 0;
		QStringList issues;  // 個別ファイルの読み込み失敗など、致命的ではない警告
	};

	// PasS の parts フォルダを「PasS互換」ライブラリとして (置き換えて) 取り込む。
	OpResult importPassFolder(const QString &sourceDir);

	// 単体の部品/基板ファイルをマイライブラリに追加する。
	// 拡張子で形式を判別する (.bpart / .part.json / .bboard)。
	OpResult importPartFile(const QString &filePath);
	OpResult importBoardFile(const QString &filePath);

	// マイライブラリに、メモリ上の Part/BoardSpec をそのまま追加/上書きする
	// (部品エディタ・基板エディタでの新規作成・編集結果の保存用)。id が既存のものと
	// 同じなら上書き (編集) になる。新規作成時は事前に uniquePartIdForMyLibrary /
	// uniqueBoardIdForMyLibrary で一意な id を発行してから設定すること。
	OpResult addPartToMyLibrary(const Part &part, const QString &categoryId = QString());
	OpResult addBoardToMyLibrary(const BoardSpec &board);
	QString uniquePartIdForMyLibrary(const QString &baseId) const;
	QString uniqueBoardIdForMyLibrary(const QString &baseId) const;

	// .blib を独立した (読み込み専用の) ライブラリとしてインストールする。
	OpResult installBlib(const QString &blibFilePath);

	// 既に読み込み済みの Library をそのままインストールする (.bpkg に同梱されていた
	// ライブラリの取込用)。id が既存のものと衝突する場合は「既にインストール済み」
	// として何もせず ok=true を返す (上書きしない)。
	OpResult installLibrary(const Library &lib);

	struct DuplicateSpec {
		QString newId;
		QString newName;
		QString newAuthor;
		QString newVersion;
		LicenseInfo newLicense;
	};
	// 複製元の内容を丸ごとコピーし、新しい id で編集可能なライブラリとして登録する。
	// 再配布ルールは newLicense から自動決定する (元が再配布不可でも、新しく設定した
	// ライセンスに従う — 派生物としての権利表示は利用者の責任)。
	// id/name/author/version を元と変える強制は UI (複製ウィザード) 側の責務とする。
	OpResult duplicateLibrary(const QString &sourceLibraryId, const DuplicateSpec &spec);

	// 編集可能なライブラリ (readOnly==false) のメタデータ (名前・バージョン・作者・
	// authorUrl・homepage・説明・ライセンス) を更新する。id と readOnly は既存の値を
	// 保つ (updated 側の値は無視する — 呼び出し側のミスで id が変わってしまうのを防ぐ)。
	// 対象が存在しないか readOnly なら false を返す。
	bool updateLibraryMetadata(const QString &id, const Library &updated);

	// マイライブラリ・PasS互換は削除できない。
	bool removeLibrary(const QString &id);

	// redistribution.allowed が false のライブラリ (マイライブラリ・PasS互換を含む) は
	// エクスポートできない。
	bool exportLibrary(const QString &id, const QString &blibFilePath) const;

signals:
	void librariesChanged();

private:
	QMap<QString, std::shared_ptr<Library>> m_libraries;

	QString libraryDir(const QString &id) const;
	bool persist(const Library &lib);
	void ensureBuiltinLibraries();
	QString uniquePartId(const Library &lib, const QString &baseId) const;
	QString uniqueBoardId(const Library &lib, const QString &baseId) const;
};
