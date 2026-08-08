#include "placeparttool.h"

#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>

#include "../../commands/placementcommands.h"
#include "../../core/ids.h"
#include "../../model/document.h"
#include "../../model/librarymanager.h"
#include "../../model/part.h"
#include "../../model/placement.h"
#include "../../render/boardscene.h"
#include "../../render/items/overlayitem.h"
#include "../input/keymap.h"
#include "snapengine.h"
#include "toolmanager.h"

bool PlacePartTool::mousePress(BoardScene *scene, QGraphicsSceneMouseEvent *event) {
	if (m_context->keymapOrDefault().matchesMouseButton(QStringLiteral("place.cancel"), event->button(),
														 event->modifiers(), InputKind::MouseButton)) {
		if (m_context->toolManager) {
			m_context->toolManager->cancelCurrent();
		}
		return true;
	}
	if (event->button() != Qt::LeftButton) {
		return false;
	}
	if (m_context->pendingPartId.isEmpty() || !m_context->libraryManager) {
		return false;
	}
	const auto part = m_context->libraryManager->resolvePart(m_context->pendingLibraryId, m_context->pendingPartId);
	if (!part) {
		return false;
	}

	const QPoint rotatedAnchor = rotatePoint(part->resolveAnchor(), part->size(), m_context->pendingRotation);
	const QPointF modelPos = scene->toModel(event->scenePos());

	auto placement = std::make_shared<Placement>();
	placement->uuid = ids::newUuid();
	placement->libraryId = m_context->pendingLibraryId;
	placement->partId = m_context->pendingPartId;
	placement->pos = m_context->snapEngine->snapForPlacement(modelPos, rotatedAnchor);
	placement->rot = m_context->pendingRotation;
	placement->side = scene->side();
	placement->refDes =
		m_context->document->nextRefDes(part->refPrefix.isEmpty() ? QStringLiteral("U") : part->refPrefix);
	placement->z = m_context->document->nextZValue();

	m_context->document->undoStack()->push(new AddPlacementCommand(m_context, placement));
	return true;
}

bool PlacePartTool::mouseMove(BoardScene *scene, QGraphicsSceneMouseEvent *event) {
	if (m_context->pendingPartId.isEmpty() || !m_context->libraryManager) {
		return false;
	}
	const auto part = m_context->libraryManager->resolvePart(m_context->pendingLibraryId, m_context->pendingPartId);
	if (!part) {
		return false;
	}

	const QSize rotSize = resolvedBoundingSize(part->size(), m_context->pendingRotation);
	const QPoint rotatedAnchor = rotatePoint(part->resolveAnchor(), part->size(), m_context->pendingRotation);
	const QPointF modelPos = scene->toModel(event->scenePos());
	const QPoint pos = m_context->snapEngine->snapForPlacement(modelPos, rotatedAnchor);

	bool valid = true;
	if (m_context->document) {
		const BoardSpec &board = m_context->document->board;
		if (board.cols > 0 && board.rows > 0) {
			const QPoint idx = board.nearestGridIndex(pos + rotatedAnchor);
			valid = board.holeExistsAt(idx.x(), idx.y());
		}
	}

	scene->overlay()->setPlacementGhost(part->artwork.image, pos, rotSize, valid);
	return true;
}

bool PlacePartTool::mouseRelease(BoardScene *, QGraphicsSceneMouseEvent *event) {
	// press だけで配置は完了しているが、対になる release もこのツールが消費した
	// ことにしておく (Qt 側のデフォルトのアイテム操作 - ラバーバンド選択の開始判定
	// など - に渡してしまわないようにするため)。
	return event->button() == Qt::LeftButton && !m_context->pendingPartId.isEmpty();
}

bool PlacePartTool::keyPress(BoardScene *, QKeyEvent *event) {
	if (m_context->keymapOrDefault().matchesKey(QStringLiteral("place.rotate"), event)) {
		m_context->pendingRotation = rotateCW(m_context->pendingRotation);
		return true;
	}
	return false;
}

bool PlacePartTool::contextMenu(BoardScene *, QGraphicsSceneContextMenuEvent *) {
	// 右クリックは mousePress 側でツール解除に使うので、標準の右クリックメニューは
	// 出さない (ここに到達した時点では既に選択ツールへ切り替わっている可能性が高いが、
	// 念のため常に消費しておく)。
	return true;
}

void PlacePartTool::deactivate() {
	if (m_context->frontScene) {
		m_context->frontScene->overlay()->clearPlacementGhost();
	}
	if (m_context->backScene) {
		m_context->backScene->overlay()->clearPlacementGhost();
	}
}

QString PlacePartTool::statusHint(const Keymap &keymap) const {
	return QObject::tr("クリックで配置 / %1で回転 / %2またはEscで解除")
		.arg(keymap.displayFor(QStringLiteral("place.rotate")), keymap.displayFor(QStringLiteral("place.cancel")));
}
