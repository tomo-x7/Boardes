#pragma once

#include <QString>
#include <QVector>

#include "../model/library.h"

// PasS の parts フォルダ (ユーザーが明示的に指定したディレクトリ) を読み込み、
// Library に変換する。
//
// PasS のバイナリ形式は実データ (241 部品 + 22 基板) を解析して解読したもので、
// PasS 本体のドキュメントには記載されていない。詳細は各 decode 関数のコメントを参照。
namespace passimport {

struct ImportIssue {
	QString file;
	QString reason;
};

struct ImportResult {
	bool ok = false;
	QString error;  // フォルダを開けない等、致命的なエラー
	int categoryCount = 0;
	int partCount = 0;
	int boardCount = 0;
	QVector<ImportIssue> issues;  // 個別ファイルの読み込み失敗 (処理は継続する)
};

// sourceDir 直下の各サブディレクトリをカテゴリとして読み込み、lib に部品・基板・
// カテゴリを追加する (lib の id/name/license 等のメタデータは呼び出し側が設定済みであること)。
ImportResult importFromDirectory(const QString &sourceDir, Library &lib);

}  // namespace passimport
