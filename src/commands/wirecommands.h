#pragma once

#include <QUndoCommand>
#include <memory>

#include "../model/wire.h"
#include "../ui/tools/toolcontext.h"

class AddWireCommand : public QUndoCommand {
public:
	AddWireCommand(ToolContext *ctx, std::shared_ptr<Wire> wire, QUndoCommand *parent = nullptr);
	void undo() override;
	void redo() override;

private:
	ToolContext *m_ctx;
	std::shared_ptr<Wire> m_wire;
};

class RemoveWireCommand : public QUndoCommand {
public:
	RemoveWireCommand(ToolContext *ctx, const QString &uuid, QUndoCommand *parent = nullptr);
	void undo() override;
	void redo() override;

private:
	ToolContext *m_ctx;
	QString m_uuid;
	std::shared_ptr<Wire> m_removed;
	int m_removedIndex = -1;
};

class ChangeWireLayerCommand : public QUndoCommand {
public:
	ChangeWireLayerCommand(ToolContext *ctx, const QString &uuid, WireLayer oldLayer, WireLayer newLayer,
						   QUndoCommand *parent = nullptr);
	void undo() override;
	void redo() override;

private:
	ToolContext *m_ctx;
	QString m_uuid;
	WireLayer m_oldLayer;
	WireLayer m_newLayer;
};

// オブジェクト一覧の目玉トグル用。表示/非表示は削除ではないので Undo 対象にする。
class SetWireVisibleCommand : public QUndoCommand {
public:
	SetWireVisibleCommand(ToolContext *ctx, const QString &uuid, bool oldVisible, bool newVisible,
						  QUndoCommand *parent = nullptr);
	void undo() override;
	void redo() override;

private:
	ToolContext *m_ctx;
	QString m_uuid;
	bool m_oldVisible;
	bool m_newVisible;
};
