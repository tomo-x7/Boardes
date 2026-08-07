#pragma once

#include <QUndoCommand>

#include "../model/board.h"
#include "../ui/tools/toolcontext.h"

// 基板の差し替え (インポート直後の初期割当や、基板編集ダイアログでの変更に使う)。
class SetBoardCommand : public QUndoCommand {
public:
	SetBoardCommand(ToolContext *ctx, BoardSpec oldBoard, BoardSpec newBoard, QUndoCommand *parent = nullptr);
	void undo() override;
	void redo() override;

private:
	ToolContext *m_ctx;
	BoardSpec m_oldBoard;
	BoardSpec m_newBoard;
};
