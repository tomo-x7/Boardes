#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <Qt>

// 1つのツールバーの構成 (Phase 19: LibreOffice 風のツールバーのカスタマイズ)。
// 「新しいツールバー」で増やしたものも、最初から用意されている既定のツールバーも
// 同じ形で表す。
struct ToolbarLayout {
	QString id;     // オブジェクト名にも使う安定した識別子 ("view" / "tools" / ユーザー作成分は uuid)
	QString title;  // 表示名
	bool builtin = false;  // 既定のツールバー (削除・id 変更不可)
	bool visible = true;
	Qt::ToolButtonStyle style = Qt::ToolButtonTextBesideIcon;
	// ActionRegistry の commandId の並び。"-" は区切り線を表す特別な値。
	QStringList items;
};

namespace toolbarlayout {

// このビルドが持つ既定のツールバー構成 (ActionRegistry に登録されている
// commandId と対応している必要がある)。
const QVector<ToolbarLayout> &defaults();

// QSettings ("toolbars") から読み込む。JSON として解析できない、または形式が
// 壊れていれば defaults() を返す (Phase 17 の設定バリデーション方針: ダイアログは
// 出さず、既定値に静かにフォールバックする)。
QVector<ToolbarLayout> load();
void save(const QVector<ToolbarLayout> &layouts);

}  // namespace toolbarlayout
