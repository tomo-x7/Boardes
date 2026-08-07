#include "boardcommands.h"

#include "../model/document.h"
#include "../render/boardscene.h"

SetBoardCommand::SetBoardCommand(ToolContext *ctx, BoardSpec oldBoard, BoardSpec newBoard, QUndoCommand *parent)
	: QUndoCommand(QObject::tr("基板の変更"), parent), m_ctx(ctx), m_oldBoard(std::move(oldBoard)),
	  m_newBoard(std::move(newBoard)) {
}

void SetBoardCommand::redo() {
	m_ctx->document->board = m_newBoard;
	if (m_ctx->frontScene) m_ctx->frontScene->syncBoard();
	if (m_ctx->backScene) m_ctx->backScene->syncBoard();
}

void SetBoardCommand::undo() {
	m_ctx->document->board = m_oldBoard;
	if (m_ctx->frontScene) m_ctx->frontScene->syncBoard();
	if (m_ctx->backScene) m_ctx->backScene->syncBoard();
}
