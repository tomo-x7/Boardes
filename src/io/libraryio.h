#pragma once

#include <QByteArray>
#include <QString>
#include <functional>
#include <optional>

#include "../model/library.h"

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
std::optional<Library> readPackage(const PackageSource &source);

bool saveToDirectory(const Library &lib, const QString &dirPath);
std::optional<Library> loadFromDirectory(const QString &dirPath);

bool exportToBlib(const Library &lib, const QString &blibFilePath);
std::optional<Library> importFromBlib(const QString &blibFilePath);

}  // namespace libraryio
