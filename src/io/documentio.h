#pragma once

#include <QJsonObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <functional>
#include <memory>

#include "../model/document.h"
#include "../model/library.h"
#include "loadresult.h"

// Document <-> .boardes (単一 JSON) / .bpkg (zip、再配布可能なライブラリを同梱)。
namespace documentio {

constexpr int SchemaVersion = 1;

QJsonObject toJsonObject(const Document &doc);
// doc は事前に構築済み (undoStack を持つ) であることを前提に、内容だけを上書きする。
void fromJsonObject(const QJsonObject &obj, Document &doc);

// obj の形式を検証したうえで doc に読み込む。形式が壊れていれば doc は変更せず
// LoadResult::ok=false を返す (「読めるところまで読んで残りを捨てる」ことはしない)。
LoadResult validateAndLoad(const QJsonObject &obj, Document &doc);

bool save(const Document &doc, const QString &filePath);
// 失敗時は LoadResult::ok=false で理由が summary/detail に入る。
LoadResult load(const QString &filePath, Document &doc);

struct PackageResult {
	bool ok = false;
	QStringList bundledLibraryIds;
	QStringList skippedLibraryIds;  // 再配布不可のため同梱できなかった依存
	QString error;
};

using LibraryResolver = std::function<std::shared_ptr<Library>(const QString &libraryId)>;

// doc.dependencies および doc.placements が参照する全ライブラリ id の和集合。
// エクスポート時にユーザーへ「どれを同梱するか」選ばせる UI (ExportPackageDialog) が使う。
QSet<QString> requiredLibraryIds(const Document &doc);

// doc.dependencies および doc.placements が参照するライブラリのうち、
// includeLibraryIds に含まれ、かつ resolver で取得できたものだけを
// "libraries/<libId>/" 以下に同梱した .bpkg を書き出す。
// includeLibraryIds は呼び出し側 (UI) が事前に選ばせる — 再配布不可のライブラリを
// 含めるかどうかはユーザーの判断であり、ここでは redistribution.allowed による
// 自動判定はしない (Phase 14: 手元のバックアップ用途を許容するため)。
PackageResult exportPackage(const Document &doc, const LibraryResolver &resolver,
							const QSet<QString> &includeLibraryIds, const QString &bpkgFilePath);

struct ImportResult {
	bool ok = false;
	QString error;
};

using LibraryImporter = std::function<void(const QString &libraryId, const Library &lib)>;

// .bpkg を開いて doc を復元する。同梱されていたライブラリはそれぞれ libraryImporter に渡すので、
// 呼び出し側 (LibraryManager) でインストールするかどうかを判断する。
ImportResult importPackage(const QString &bpkgFilePath, Document &doc, const LibraryImporter &libraryImporter);

}  // namespace documentio
