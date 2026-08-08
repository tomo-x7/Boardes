#include "wirecommands.h"

#include "../model/document.h"

AddWireCommand::AddWireCommand(ToolContext *ctx, std::shared_ptr<Wire> wire, QUndoCommand *parent)
	: QUndoCommand(QObject::tr("配線の追加"), parent), m_ctx(ctx), m_wire(std::move(wire)) {
}

void AddWireCommand::redo() {
	m_ctx->document->wires.append(m_wire);
	m_ctx->syncBothScenesWires();
}

void AddWireCommand::undo() {
	const int idx = m_ctx->document->indexOfWire(m_wire->uuid);
	if (idx >= 0) {
		m_ctx->document->wires.removeAt(idx);
	}
	m_ctx->syncBothScenesWires();
}

RemoveWireCommand::RemoveWireCommand(ToolContext *ctx, const QString &uuid, QUndoCommand *parent)
	: QUndoCommand(QObject::tr("配線の削除"), parent), m_ctx(ctx), m_uuid(uuid) {
}

void RemoveWireCommand::redo() {
	m_removedIndex = m_ctx->document->indexOfWire(m_uuid);
	if (m_removedIndex >= 0) {
		m_removed = m_ctx->document->wires[m_removedIndex];
		m_ctx->document->wires.removeAt(m_removedIndex);
	}
	m_ctx->syncBothScenesWires();
}

void RemoveWireCommand::undo() {
	if (m_removed) {
		const int insertAt = qBound(0, m_removedIndex, m_ctx->document->wires.size());
		m_ctx->document->wires.insert(insertAt, m_removed);
	}
	m_ctx->syncBothScenesWires();
}

ChangeWireLayerCommand::ChangeWireLayerCommand(ToolContext *ctx, const QString &uuid, WireLayer oldLayer,
											   WireLayer newLayer, QUndoCommand *parent)
	: QUndoCommand(QObject::tr("配線の線種変更"), parent), m_ctx(ctx), m_uuid(uuid), m_oldLayer(oldLayer),
	  m_newLayer(newLayer) {
}

void ChangeWireLayerCommand::redo() {
	const int idx = m_ctx->document->indexOfWire(m_uuid);
	if (idx >= 0) {
		m_ctx->document->wires[idx]->layer = m_newLayer;
	}
	m_ctx->syncBothScenesWires();
}

void ChangeWireLayerCommand::undo() {
	const int idx = m_ctx->document->indexOfWire(m_uuid);
	if (idx >= 0) {
		m_ctx->document->wires[idx]->layer = m_oldLayer;
	}
	m_ctx->syncBothScenesWires();
}

SetWireVisibleCommand::SetWireVisibleCommand(ToolContext *ctx, const QString &uuid, bool oldVisible, bool newVisible,
											 QUndoCommand *parent)
	: QUndoCommand(newVisible ? QObject::tr("配線を表示") : QObject::tr("配線を非表示"), parent), m_ctx(ctx),
	  m_uuid(uuid), m_oldVisible(oldVisible), m_newVisible(newVisible) {
}

void SetWireVisibleCommand::redo() {
	const int idx = m_ctx->document->indexOfWire(m_uuid);
	if (idx >= 0) {
		m_ctx->document->wires[idx]->visible = m_newVisible;
	}
	m_ctx->syncBothScenesWires();
}

void SetWireVisibleCommand::undo() {
	const int idx = m_ctx->document->indexOfWire(m_uuid);
	if (idx >= 0) {
		m_ctx->document->wires[idx]->visible = m_oldVisible;
	}
	m_ctx->syncBothScenesWires();
}
