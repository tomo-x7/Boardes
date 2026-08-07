#include "placeparttool.h"

#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>

#include "../../commands/placementcommands.h"
#include "../../core/ids.h"
#include "../../model/document.h"
#include "../../model/librarymanager.h"
#include "../../model/part.h"
#include "../../render/boardscene.h"
#include "snapengine.h"

bool PlacePartTool::mousePress(BoardScene *scene, QGraphicsSceneMouseEvent *event) {
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

	auto placement = std::make_shared<Placement>();
	placement->uuid = ids::newUuid();
	placement->libraryId = m_context->pendingLibraryId;
	placement->partId = m_context->pendingPartId;
	placement->pos = SnapEngine::snapForPlacement(event->scenePos());
	placement->rot = m_context->pendingRotation;
	placement->side = scene->side();
	placement->refDes =
		m_context->document->nextRefDes(part->refPrefix.isEmpty() ? QStringLiteral("U") : part->refPrefix);
	placement->z = m_context->document->nextZValue();

	m_context->document->undoStack()->push(new AddPlacementCommand(m_context, placement));
	return true;
}

bool PlacePartTool::mouseRelease(BoardScene *, QGraphicsSceneMouseEvent *event) {
	// press だけで配置は完了しているが、対になる release もこのツールが消費した
	// ことにしておく (Qt 側のデフォルトのアイテム操作 - ラバーバンド選択の開始判定
	// など - に渡してしまわないようにするため)。
	return event->button() == Qt::LeftButton && !m_context->pendingPartId.isEmpty();
}

bool PlacePartTool::keyPress(BoardScene *, QKeyEvent *event) {
	if (event->key() == Qt::Key_R) {
		m_context->pendingRotation = rotateCW(m_context->pendingRotation);
		return true;
	}
	return false;
}

QString PlacePartTool::statusHint() const {
	return QObject::tr("クリックで部品を配置 / R キーで回転 / Esc でツール解除");
}
