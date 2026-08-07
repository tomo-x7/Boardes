#pragma once

#include <QString>
#include <QUndoStack>
#include <QVector>
#include <memory>

#include "board.h"
#include "placement.h"
#include "wire.h"

// 設計データが依存しているライブラリの記録。
// エクスポート時に再配布可否を判定するために保持する。
struct Dependency {
	QString libraryId;
	QString name;
	QString version;
	QString licenseSpdx;  // 空 = ライセンス不明/独自
	bool redistributable = false;
};

// 1つの設計ドキュメント (.boardes の中身)。
class Document {
public:
	Document();

	QString title;
	QString author;
	QString notes;

	BoardSpec board;  // 自己完結のため実体を埋め込む (パッケージ化・単一JSON保存を単純にする)

	// shared_ptr で持つ理由: 描画アイテム (PlacementItem/WireItem) や QUndoCommand が
	// 個々の要素への参照を安全に保持し続けられるようにするため。QVector<Placement> の
	// ような値保持だと、他要素の追加/削除に伴う配列の再配置でポインタ/参照が
	// 無効になってしまう (shared_ptr ならベクタ自体が再配置されても指す先は動かない)。
	QVector<std::shared_ptr<Placement>> placements;
	QVector<std::shared_ptr<Wire>> wires;
	QVector<Dependency> dependencies;

	// 編集操作はすべて QUndoCommand 経由でこのスタックに積む。
	// isClean() をそのままダーティフラグとして扱う。
	QUndoStack *undoStack() {
		return m_undoStack.get();
	}

	int indexOfPlacement(const QString &uuid) const;
	int indexOfWire(const QString &uuid) const;

	// 依存ライブラリ一覧に (無ければ) 追加する。
	void ensureDependency(const Dependency &dep);

	// prefix (例 "R") を使っている refDes の最大番号+1 を返す。使用例が無ければ 1。
	QString nextRefDes(const QString &prefix) const;

	// 新規配置に使う z 値 (既存の最大値+1。後に置いたものが上に表示される)。
	int nextZValue() const;

private:
	std::unique_ptr<QUndoStack> m_undoStack;
};
