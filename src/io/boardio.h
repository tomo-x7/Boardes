#pragma once

#include <QJsonObject>
#include <QString>
#include <optional>

#include "../model/board.h"
#include "loadresult.h"

// BoardSpec <-> JSON。背景画像は常に base64 埋め込み (基板は単体でも設計データ内でも
// 自己完結して読めるようにするため、部品と違いサイドカー形式は用意しない)。
namespace boardio {

constexpr int SchemaVersion = 1;

QJsonObject toJsonObject(const BoardSpec &board);
BoardSpec fromJsonObject(const QJsonObject &obj);

// obj が正しい形式かどうかを検証する。documentio (埋め込みの board) からも
// 単体の .bboard からも同じ検証を使う。path はエラー表示用の接頭辞
// (例: "board" や "boards[2]")。
LoadResult validateJson(const QJsonObject &obj, const QString &path);

// .bboard 単体ファイル I/O。失敗時、errorOut が渡されていれば理由を書き込む。
bool save(const BoardSpec &board, const QString &filePath);
std::optional<BoardSpec> load(const QString &filePath, LoadResult *errorOut = nullptr);

}  // namespace boardio
