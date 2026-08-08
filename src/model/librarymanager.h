#pragma once

#include <QMap>
#include <QObject>
#include <QSet>
#include <QString>
#include <memory>
#include <optional>

#include "library.h"

// 実行時に読み込まれている全ライブラリを管理するレジストリ。
//
// 常に以下が存在する:
//   - マイライブラリ (id: myLibraryId())    既定で編集可 / export 不可 (再配布不可ライセンスのため)
//   - PasS互換       (id: passCompatId())   編集可 / export 可能だが再配布不可の警告が出る
// それに加えて、.blib からインストールしたライブラリや、複製で作った
// ライブラリが増えていく。Phase 14 以降、readOnly の区別は廃止した
// (手元にあるライブラリはすべて編集・バックアップが自由。公開・再配布の可否だけを
// redistribution.allowed で管理する)。
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

	// loadAll() 中に読み込めなかったライブラリディレクトリの一覧 (dir, reason)。
	// 起動直後にまとめて警告するために MainWindow が使う (Phase 17)。
	struct LoadIssue {
		QString directory;
		QString reason;
	};
	QVector<LoadIssue> loadIssues() const {
		return m_loadIssues;
	}

	// PasS の parts フォルダを「PasS互換」ライブラリとして (置き換えて) 取り込む。
	OpResult importPassFolder(const QString &sourceDir);

	// 単体の部品/基板ファイルをマイライブラリに追加する。
	// 拡張子で形式を判別する (.bpart / .part.json / .bboard)。
	OpResult importPartFile(const QString &filePath);
	OpResult importBoardFile(const QString &filePath);

	// --- ライブラリ ---
	// 空のライブラリを新規作成する (id 重複はエラー)。
	OpResult createLibrary(const Library &meta);
	// メタデータ (名前・バージョン・作者・authorUrl・homepage・説明・ライセンス) を更新する。
	// id は既存の値を保つ (updated 側の値は無視する — 呼び出し側のミスで id が変わって
	// しまうのを防ぐ)。Phase 14 で readOnly チェックは廃止 (全ライブラリ編集可)。
	bool updateLibraryMetadata(const QString &id, const Library &updated);
	// マイライブラリ・PasS互換を含め、どのライブラリも削除できる (確認は呼び出し側の責務)。
	bool removeLibrary(const QString &id);

	// --- カテゴリ ---
	bool addCategory(const QString &libId, const CategoryInfo &cat);
	bool updateCategory(const QString &libId, const CategoryInfo &cat);  // id で照合
	bool removeCategory(const QString &libId, const QString &catId);    // 中の部品は未分類へ移す
	bool reorderCategories(const QString &libId, const QStringList &orderedIds);

	// マイライブラリに、メモリ上の Part/BoardSpec をそのまま追加/上書きする
	// (部品エディタ・基板エディタでの新規作成・編集結果の保存用)。id が既存のものと
	// 同じなら上書き (編集) になる。新規作成時は事前に uniquePartIdForMyLibrary /
	// uniqueBoardIdForMyLibrary で一意な id を発行してから設定すること。
	OpResult addPartToMyLibrary(const Part &part, const QString &categoryId = QString());
	OpResult addBoardToMyLibrary(const BoardSpec &board);
	QString uniquePartIdForMyLibrary(const QString &baseId) const;
	QString uniqueBoardIdForMyLibrary(const QString &baseId) const;

	// --- 任意のライブラリへの部品・基板の追加/削除/移動 ---
	OpResult addPartTo(const QString &libId, const Part &part, const QString &categoryId = QString());
	bool removePartFrom(const QString &libId, const QString &partId);
	bool setPartCategory(const QString &libId, const QString &partId, const QString &categoryId);
	OpResult addBoardTo(const QString &libId, const BoardSpec &board);
	bool removeBoardFrom(const QString &libId, const QString &boardId);
	QString uniquePartId(const QString &libId, const QString &baseId) const;
	QString uniqueBoardId(const QString &libId, const QString &baseId) const;

	// 別ライブラリから部品/基板を複製する。id 衝突は uniquePartId/uniqueBoardId で回避。
	// 複製元が redistribution.allowed==false の場合、複製先の basedOn に記録を残す
	// (呼び出し側 UI が事前に警告を出すこと)。
	OpResult copyPartsBetween(const QString &srcLibId, const QStringList &partIds, const QString &dstLibId,
							  const QString &dstCategoryId);
	OpResult copyBoardsBetween(const QString &srcLibId, const QStringList &boardIds, const QString &dstLibId);

	// .blib を独立したライブラリとしてインストールする。
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
		RedistributionRule newRedistribution;  // newLicense.kind==Custom のときの手動指定値
	};
	// 複製元の内容を丸ごとコピーし、新しい id で登録する。派生ライセンス強制の仕組みは
	// 持たない (spec.newLicense/newRedistribution をそのまま使う)。
	// id/name/author/version を元と変える強制は UI (複製ウィザード) 側の責務とする。
	OpResult duplicateLibrary(const QString &sourceLibraryId, const DuplicateSpec &spec);

	// redistribution.allowed が false のライブラリでも書き出せる (手元のバックアップ用途)。
	// 再配布不可であることをユーザーに警告するのは呼び出し側 (UI) の責務。
	bool exportLibrary(const QString &id, const QString &blibFilePath) const;

signals:
	void librariesChanged();

private:
	QMap<QString, std::shared_ptr<Library>> m_libraries;
	QVector<LoadIssue> m_loadIssues;

	QString libraryDir(const QString &id) const;
	bool persist(const Library &lib);
	void ensureBuiltinLibraries();
	QString uniquePartIdIn(const Library &lib, const QString &baseId) const;
	QString uniqueBoardIdIn(const Library &lib, const QString &baseId) const;
	// id の Library をコピーして取り出し、mutator を適用した後 persist + キャッシュ更新
	// + librariesChanged を行う共通処理 (追加/削除/更新系メソッドの重複を減らす)。
	// mutator が false を返したら中断し、永続化しない。
	template <class F>
	bool mutateLibrary(const QString &libId, F mutator);
};
