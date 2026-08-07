#include "placementcommands.h"

#include "../model/document.h"
#include "../model/librarymanager.h"

namespace {
// 部品が参照しているライブラリを、設計データの依存関係一覧に記録する
// (.bpkg エクスポート時にどのライブラリを同梱すべきか判断するために使う)。
void ensureDependencyFor(ToolContext *ctx, const QString &libraryId) {
	if (!ctx->libraryManager) {
		return;
	}
	const auto lib = ctx->libraryManager->library(libraryId);
	if (!lib) {
		return;
	}
	Dependency dep;
	dep.libraryId = lib->id;
	dep.name = lib->name;
	dep.version = lib->version;
	dep.licenseSpdx = licenseSpdxId(lib->license.kind);
	dep.redistributable = lib->redistribution.allowed;
	ctx->document->ensureDependency(dep);
}
}  // namespace

AddPlacementCommand::AddPlacementCommand(ToolContext *ctx, std::shared_ptr<Placement> placement,
										 QUndoCommand *parent)
	: QUndoCommand(QObject::tr("部品の配置"), parent), m_ctx(ctx), m_placement(std::move(placement)) {
}

void AddPlacementCommand::redo() {
	m_ctx->document->placements.append(m_placement);
	ensureDependencyFor(m_ctx, m_placement->libraryId);
	m_ctx->syncBothScenesPlacements();
}

void AddPlacementCommand::undo() {
	const int idx = m_ctx->document->indexOfPlacement(m_placement->uuid);
	if (idx >= 0) {
		m_ctx->document->placements.removeAt(idx);
	}
	m_ctx->syncBothScenesPlacements();
}

RemovePlacementCommand::RemovePlacementCommand(ToolContext *ctx, const QString &uuid, QUndoCommand *parent)
	: QUndoCommand(QObject::tr("部品の削除"), parent), m_ctx(ctx), m_uuid(uuid) {
}

void RemovePlacementCommand::redo() {
	m_removedIndex = m_ctx->document->indexOfPlacement(m_uuid);
	if (m_removedIndex >= 0) {
		m_removed = m_ctx->document->placements[m_removedIndex];
		m_ctx->document->placements.removeAt(m_removedIndex);
	}
	m_ctx->syncBothScenesPlacements();
}

void RemovePlacementCommand::undo() {
	if (m_removed) {
		const int insertAt = qBound(0, m_removedIndex, m_ctx->document->placements.size());
		m_ctx->document->placements.insert(insertAt, m_removed);
	}
	m_ctx->syncBothScenesPlacements();
}

MovePlacementCommand::MovePlacementCommand(ToolContext *ctx, const QString &uuid, QPoint oldPos, QPoint newPos,
										   QUndoCommand *parent)
	: QUndoCommand(QObject::tr("部品の移動"), parent), m_ctx(ctx), m_uuid(uuid), m_oldPos(oldPos), m_newPos(newPos) {
}

void MovePlacementCommand::redo() {
	const int idx = m_ctx->document->indexOfPlacement(m_uuid);
	if (idx >= 0) {
		m_ctx->document->placements[idx]->pos = m_newPos;
	}
	m_ctx->syncBothScenesPlacements();
}

void MovePlacementCommand::undo() {
	const int idx = m_ctx->document->indexOfPlacement(m_uuid);
	if (idx >= 0) {
		m_ctx->document->placements[idx]->pos = m_oldPos;
	}
	m_ctx->syncBothScenesPlacements();
}

RotatePlacementCommand::RotatePlacementCommand(ToolContext *ctx, const QString &uuid, Rotation oldRot,
											   Rotation newRot, QUndoCommand *parent)
	: QUndoCommand(QObject::tr("部品の回転"), parent), m_ctx(ctx), m_uuid(uuid), m_oldRot(oldRot), m_newRot(newRot) {
}

void RotatePlacementCommand::redo() {
	const int idx = m_ctx->document->indexOfPlacement(m_uuid);
	if (idx >= 0) {
		m_ctx->document->placements[idx]->rot = m_newRot;
	}
	m_ctx->syncBothScenesPlacements();
}

void RotatePlacementCommand::undo() {
	const int idx = m_ctx->document->indexOfPlacement(m_uuid);
	if (idx >= 0) {
		m_ctx->document->placements[idx]->rot = m_oldRot;
	}
	m_ctx->syncBothScenesPlacements();
}

FlipPlacementSideCommand::FlipPlacementSideCommand(ToolContext *ctx, const QString &uuid, Side oldSide,
												   Side newSide, QUndoCommand *parent)
	: QUndoCommand(QObject::tr("表裏の切替"), parent), m_ctx(ctx), m_uuid(uuid), m_oldSide(oldSide),
	  m_newSide(newSide) {
}

void FlipPlacementSideCommand::redo() {
	const int idx = m_ctx->document->indexOfPlacement(m_uuid);
	if (idx >= 0) {
		m_ctx->document->placements[idx]->side = m_newSide;
	}
	m_ctx->syncBothScenesPlacements();
}

void FlipPlacementSideCommand::undo() {
	const int idx = m_ctx->document->indexOfPlacement(m_uuid);
	if (idx >= 0) {
		m_ctx->document->placements[idx]->side = m_oldSide;
	}
	m_ctx->syncBothScenesPlacements();
}

SetPlacementLabelCommand::SetPlacementLabelCommand(ToolContext *ctx, const QString &uuid, QString oldRefDes,
												   QString oldValue, QString newRefDes, QString newValue,
												   QUndoCommand *parent)
	: QUndoCommand(QObject::tr("ラベルの変更"), parent),
	  m_ctx(ctx),
	  m_uuid(uuid),
	  m_oldRefDes(std::move(oldRefDes)),
	  m_oldValue(std::move(oldValue)),
	  m_newRefDes(std::move(newRefDes)),
	  m_newValue(std::move(newValue)) {
}

void SetPlacementLabelCommand::redo() {
	const int idx = m_ctx->document->indexOfPlacement(m_uuid);
	if (idx >= 0) {
		m_ctx->document->placements[idx]->refDes = m_newRefDes;
		m_ctx->document->placements[idx]->value = m_newValue;
	}
	m_ctx->syncBothScenesPlacements();
}

void SetPlacementLabelCommand::undo() {
	const int idx = m_ctx->document->indexOfPlacement(m_uuid);
	if (idx >= 0) {
		m_ctx->document->placements[idx]->refDes = m_oldRefDes;
		m_ctx->document->placements[idx]->value = m_oldValue;
	}
	m_ctx->syncBothScenesPlacements();
}
