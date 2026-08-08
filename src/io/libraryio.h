#pragma once

#include <QByteArray>
#include <QString>
#include <functional>
#include <optional>

#include "../model/library.h"
#include "loadresult.h"

// Library のパッケージ形式 I/O。
//
// ディレクトリ展開形と .blib (zip) 形式は中身のレイアウトが同一なので、
// 読み書きロジックは PackageSource/PackageSink という抽象を介して1つだけ用意し、
// ディレクトリ版と zip 版はその薄いアダプタとして実装する。
//
// パッケージレイアウト:
//   library.json
//   categories/<catId>/icon.png
//   parts/<partId>.part.json  (+ parts/<partId>.png サイドカー)
//   boards/<boardId>.bboard
//   LICENSE.txt                (license.customLicenseText が非空のときのみ)
namespace libraryio {

constexpr int SchemaVersion = 1;

struct PackageSource {
	std::function<QByteArray(const QString &relPath)> read;
	std::function<bool(const QString &relPath)> exists;
};

struct PackageSink {
	std::function<bool(const QString &relPath, const QByteArray &data)> write;
};

bool writePackage(const Library &lib, const PackageSink &sink);
// 失敗時、errorOut が渡されていれば理由を書き込む。参照先の .bpart/.bboard を
// 1件でも読めなければ、ライブラリ全体を読み込み失敗として返す (Phase 17)。
// 致命的ではない問題 (未知のカテゴリを参照する部品、デコードできないアイコン等) は
// errorOut->warnings に積んだうえで読み込みは続行する。
std::optional<Library> readPackage(const PackageSource &source, LoadResult *errorOut = nullptr);

bool saveToDirectory(const Library &lib, const QString &dirPath);
std::optional<Library> loadFromDirectory(const QString &dirPath, LoadResult *errorOut = nullptr);

bool exportToBlib(const Library &lib, const QString &blibFilePath);
std::optional<Library> importFromBlib(const QString &blibFilePath, LoadResult *errorOut = nullptr);

}  // namespace libraryio
