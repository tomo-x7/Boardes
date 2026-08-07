#pragma once

#include <QJsonObject>
#include <QString>
#include <optional>

#include "../model/board.h"

// BoardSpec <-> JSON。背景画像は常に base64 埋め込み (基板は単体でも設計データ内でも
// 自己完結して読めるようにするため、部品と違いサイドカー形式は用意しない)。
namespace boardio {

constexpr int SchemaVersion = 1;

QJsonObject toJsonObject(const BoardSpec &board);
BoardSpec fromJsonObject(const QJsonObject &obj);

// .bboard 単体ファイル I/O。
bool save(const BoardSpec &board, const QString &filePath);
std::optional<BoardSpec> load(const QString &filePath);

}  // namespace boardio
