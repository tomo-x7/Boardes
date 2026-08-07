#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <functional>
#include <memory>

#include "../model/document.h"
#include "../model/library.h"

// Document <-> .boardes (単一 JSON) / .bpkg (zip、再配布可能なライブラリを同梱)。
namespace documentio {

constexpr int SchemaVersion = 1;

QJsonObject toJsonObject(const Document &doc);
// doc は事前に構築済み (undoStack を持つ) であることを前提に、内容だけを上書きする。
void fromJsonObject(const QJsonObject &obj, Document &doc);

bool save(const Document &doc, const QString &filePath);
bool load(const QString &filePath, Document &doc);

struct PackageResult {
	bool ok = false;
	QStringList bundledLibraryIds;
	QStringList skippedLibraryIds;  // 再配布不可のため同梱できなかった依存
	QString error;
};

using LibraryResolver = std::function<std::shared_ptr<Library>(const QString &libraryId)>;

// doc.dependencies のうち、resolver で取得できてかつ redistribution.allowed な
// ライブラリだけを "libraries/<libId>/" 以下に同梱した .bpkg を書き出す。
PackageResult exportPackage(const Document &doc, const LibraryResolver &resolver, const QString &bpkgFilePath);

struct ImportResult {
	bool ok = false;
	QString error;
};

using LibraryImporter = std::function<void(const QString &libraryId, const Library &lib)>;

// .bpkg を開いて doc を復元する。同梱されていたライブラリはそれぞれ libraryImporter に渡すので、
// 呼び出し側 (LibraryManager) でインストールするかどうかを判断する。
ImportResult importPackage(const QString &bpkgFilePath, Document &doc, const LibraryImporter &libraryImporter);

}  // namespace documentio
