#pragma once

#include <QJsonObject>
#include <QString>
#include <functional>
#include <optional>

#include "../model/part.h"

// Part <-> JSON。
//
// 部品の図形データは「JSON + サイドカー画像」と「JSON + base64 埋め込み」の
// どちらでも表現できる (schema は同じで "artwork.encoding" だけが違う)。
// ライブラリ (.blib) の中身は容量節約のためサイドカー形式、単体エクスポートの
// .bpart は取り回しの良さのため埋め込み形式、という使い分けを想定している。
namespace partio {

constexpr int SchemaVersion = 1;

// encoding=="file" のとき参照される相対パスからバイト列を取得するための callback。
// ディレクトリからでも zip アーカイブからでも同じ形で対応できるようにするための抽象化。
using ArtworkResolver = std::function<QByteArray(const QString &relativePath)>;

// artworkFileName を空にすると "<id>.png" を既定値として使う。
QJsonObject toJsonObject(const Part &part, bool embedBase64, const QString &artworkFileName = QString());

// resolver は embedBase64==false (encoding=="file") のときのみ呼ばれる。
Part fromJsonObject(const QJsonObject &obj, const ArtworkResolver &resolver);

QString defaultArtworkFileName(const Part &part);

// 単体ファイル I/O (アプリの「部品をエクスポート/インポート」機能用)。

// .bpart: JSON 1ファイル、画像は base64 埋め込み。
bool saveEmbedded(const Part &part, const QString &bpartFilePath);
std::optional<Part> loadEmbedded(const QString &bpartFilePath);

// <name>.part.json + サイドカー画像 (既定 <name>.png)。
bool saveSidecar(const Part &part, const QString &jsonFilePath);
std::optional<Part> loadSidecar(const QString &jsonFilePath);

}  // namespace partio
