#pragma once

#include <QString>
#include <QVector>

#include "inputgesture.h"

// カスタマイズ可能な「操作」1つの定義。id はキーマップの保存キーとしても使う、
// 安定した識別子 (例: "wire.commit")。category は衝突判定の単位で、同一カテゴリ内
// および "global" との重複を Keymap::conflicts() が検出する (Phase 18)。
struct CommandDef {
	QString id;
	QString category;
	QString label;
	QString description;
	QVector<InputGesture> defaults;
};

namespace commandregistry {

// 全コマンド定義。カテゴリ・定義順に並んでいる (KeymapDialog の表示順としても使う)。
const QVector<CommandDef> &allCommands();
const CommandDef *find(const QString &commandId);

}  // namespace commandregistry
