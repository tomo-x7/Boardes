#pragma once

#include <QUndoCommand>
#include <memory>

#include "../model/placement.h"
#include "../ui/tools/toolcontext.h"

// Placement 系の編集はすべて (document->placements を直接書き換え → 両シーンを
// syncPlacements() で同期) というパターンなので、各コマンドはそれを redo/undo の
// 両方で行うだけの薄いラッパになる。

class AddPlacementCommand : public QUndoCommand {
public:
	AddPlacementCommand(ToolContext *ctx, std::shared_ptr<Placement> placement, QUndoCommand *parent = nullptr);
	void undo() override;
	void redo() override;

private:
	ToolContext *m_ctx;
	std::shared_ptr<Placement> m_placement;
};

class RemovePlacementCommand : public QUndoCommand {
public:
	RemovePlacementCommand(ToolContext *ctx, const QString &uuid, QUndoCommand *parent = nullptr);
	void undo() override;
	void redo() override;

private:
	ToolContext *m_ctx;
	QString m_uuid;
	std::shared_ptr<Placement> m_removed;
	int m_removedIndex = -1;
};

class MovePlacementCommand : public QUndoCommand {
public:
	MovePlacementCommand(ToolContext *ctx, const QString &uuid, QPoint oldPos, QPoint newPos,
						 QUndoCommand *parent = nullptr);
	void undo() override;
	void redo() override;

private:
	ToolContext *m_ctx;
	QString m_uuid;
	QPoint m_oldPos;
	QPoint m_newPos;
};

class RotatePlacementCommand : public QUndoCommand {
public:
	RotatePlacementCommand(ToolContext *ctx, const QString &uuid, Rotation oldRot, Rotation newRot,
						   QUndoCommand *parent = nullptr);
	void undo() override;
	void redo() override;

private:
	ToolContext *m_ctx;
	QString m_uuid;
	Rotation m_oldRot;
	Rotation m_newRot;
};

class FlipPlacementSideCommand : public QUndoCommand {
public:
	FlipPlacementSideCommand(ToolContext *ctx, const QString &uuid, Side oldSide, Side newSide,
							 QUndoCommand *parent = nullptr);
	void undo() override;
	void redo() override;

private:
	ToolContext *m_ctx;
	QString m_uuid;
	Side m_oldSide;
	Side m_newSide;
};

class SetPlacementLabelCommand : public QUndoCommand {
public:
	SetPlacementLabelCommand(ToolContext *ctx, const QString &uuid, QString oldRefDes, QString oldValue,
							 QString newRefDes, QString newValue, QUndoCommand *parent = nullptr);
	void undo() override;
	void redo() override;

private:
	ToolContext *m_ctx;
	QString m_uuid;
	QString m_oldRefDes, m_oldValue, m_newRefDes, m_newValue;
};
