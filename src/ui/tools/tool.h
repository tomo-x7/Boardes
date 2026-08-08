#pragma once

#include <QString>

#include "toolcontext.h"

class BoardScene;
class QGraphicsSceneMouseEvent;
class QGraphicsSceneContextMenuEvent;
class QKeyEvent;
class Keymap;

// 編集ツールの基底クラス。BoardScene からのマウス/キー入力を受け取り、
// Document を QUndoCommand 経由で編集する。
//
// 各 handle* は「このイベントを消費したか」を返す。true ならシーンの
// デフォルト処理 (ラバーバンド選択など) を行わない。
class Tool {
public:
	explicit Tool(ToolContext *context) : m_context(context) {
	}
	virtual ~Tool() = default;

	virtual void activate() {
	}
	virtual void deactivate() {
	}

	virtual bool mousePress(BoardScene *scene, QGraphicsSceneMouseEvent *event) {
		Q_UNUSED(scene);
		Q_UNUSED(event);
		return false;
	}
	virtual bool mouseMove(BoardScene *scene, QGraphicsSceneMouseEvent *event) {
		Q_UNUSED(scene);
		Q_UNUSED(event);
		return false;
	}
	virtual bool mouseRelease(BoardScene *scene, QGraphicsSceneMouseEvent *event) {
		Q_UNUSED(scene);
		Q_UNUSED(event);
		return false;
	}
	virtual bool mouseDoubleClick(BoardScene *scene, QGraphicsSceneMouseEvent *event) {
		Q_UNUSED(scene);
		Q_UNUSED(event);
		return false;
	}
	virtual bool keyPress(BoardScene *scene, QKeyEvent *event) {
		Q_UNUSED(scene);
		Q_UNUSED(event);
		return false;
	}
	virtual bool contextMenu(BoardScene *scene, QGraphicsSceneContextMenuEvent *event) {
		Q_UNUSED(scene);
		Q_UNUSED(event);
		return false;
	}

	// ツール内部の途中状態 (作図中の配線・選択範囲など) を破棄する。破棄すべきものが
	// あった場合のみ true を返す。false ならツール自体を解除してよい、という意味
	// (ToolManager::cancelCurrent() が Esc の共通処理として使う)。
	virtual bool cancel() {
		return false;
	}

	// ステータスバーに出す操作ヒント。keymap のカスタマイズ結果を文言に反映させる
	// (Phase 18)。GIMP 風のリアルタイム操作ヒント (左下) はここが唯一の情報源。
	virtual QString statusHint(const Keymap &keymap) const = 0;

protected:
	ToolContext *m_context;
};
