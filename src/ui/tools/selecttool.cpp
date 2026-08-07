#include "selecttool.h"

#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <climits>

#include "../../commands/placementcommands.h"
#include "../../commands/wirecommands.h"
#include "../../core/ids.h"
#include "../../model/document.h"
#include "../../render/boardscene.h"
#include "../../render/items/placementitem.h"
#include "../../render/items/wireitem.h"
#include "snapengine.h"

SelectTool::SelectTool(ToolContext *context) : QObject(nullptr), Tool(context) {
}

void SelectTool::activate() {
	if (m_context->frontScene) {
		connect(m_context->frontScene, &QGraphicsScene::selectionChanged, this, &SelectTool::onSelectionChanged);
	}
	if (m_context->backScene) {
		connect(m_context->backScene, &QGraphicsScene::selectionChanged, this, &SelectTool::onSelectionChanged);
	}
}

void SelectTool::deactivate() {
	if (m_context->frontScene) {
		disconnect(m_context->frontScene, nullptr, this, nullptr);
	}
	if (m_context->backScene) {
		disconnect(m_context->backScene, nullptr, this, nullptr);
	}
	m_dragging = false;
	m_dragEntries.clear();
	clearNetHighlight();
}

void SelectTool::onSelectionChanged() {
	if (m_syncingSelection) {
		return;
	}
	auto *senderScene = qobject_cast<BoardScene *>(sender());
	if (!senderScene) {
		return;
	}
	syncSelectionAcrossScenes(senderScene);
}

void SelectTool::syncSelectionAcrossScenes(BoardScene *changedScene) {
	BoardScene *other =
		(changedScene == m_context->frontScene) ? m_context->backScene : m_context->frontScene;
	if (!other) {
		return;
	}

	QSet<QString> placementUuids;
	QSet<QString> wireUuids;
	for (QGraphicsItem *item : changedScene->selectedItems()) {
		if (auto *pi = dynamic_cast<PlacementItem *>(item)) {
			placementUuids.insert(pi->placement()->uuid);
		} else if (auto *wi = dynamic_cast<WireItem *>(item)) {
			wireUuids.insert(wi->wire()->uuid);
		}
	}

	m_syncingSelection = true;
	for (auto *pi : other->allPlacementItems()) {
		pi->setSelected(placementUuids.contains(pi->placement()->uuid));
	}
	for (auto *wi : other->allWireItems()) {
		wi->setSelected(wireUuids.contains(wi->wire()->uuid));
	}
	m_syncingSelection = false;
}

QVector<QString> SelectTool::selectedPlacementUuids() const {
	QVector<QString> out;
	if (!m_context->frontScene) {
		return out;
	}
	for (QGraphicsItem *item : m_context->frontScene->selectedItems()) {
		if (auto *pi = dynamic_cast<PlacementItem *>(item)) {
			out.append(pi->placement()->uuid);
		}
	}
	return out;
}

QVector<QString> SelectTool::selectedWireUuids() const {
	QVector<QString> out;
	if (!m_context->frontScene) {
		return out;
	}
	for (QGraphicsItem *item : m_context->frontScene->selectedItems()) {
		if (auto *wi = dynamic_cast<WireItem *>(item)) {
			out.append(wi->wire()->uuid);
		}
	}
	return out;
}

void SelectTool::clearNetHighlight() {
	m_lastHoveredWireUuid.clear();
	for (BoardScene *scene : {m_context->frontScene, m_context->backScene}) {
		if (!scene) continue;
		for (auto *w : scene->allWireItems()) {
			w->setNetHighlighted(false);
		}
	}
}

void SelectTool::updateNetHighlight(BoardScene *scene, QGraphicsSceneMouseEvent *event) {
	if (!scene) {
		return;
	}
	QGraphicsItem *hit = scene->itemAt(event->scenePos(), QTransform());
	auto *wireItem = dynamic_cast<WireItem *>(hit);
	const QString hoveredUuid = wireItem ? wireItem->wire()->uuid : QString();
	if (hoveredUuid == m_lastHoveredWireUuid) {
		return;  // ホバー対象が変わっていなければ何もしない (Union-Find の再計算を避ける)
	}
	clearNetHighlight();
	m_lastHoveredWireUuid = hoveredUuid;
	if (hoveredUuid.isEmpty() || !m_context->document) {
		return;
	}

	m_netlist.rebuild(*m_context->document, m_context->libraryManager);
	const int idx = m_context->document->indexOfWire(hoveredUuid);
	if (idx < 0) {
		return;
	}
	const auto &hoveredWire = m_context->document->wires[idx];
	if (hoveredWire->points.isEmpty()) {
		return;
	}
	const auto side = wireSide(hoveredWire->layer);
	if (!side.has_value()) {
		return;  // 外形線はネットに属さない
	}
	const int netId = m_netlist.netIdAt(hoveredWire->points.first(), *side);
	if (netId < 0) {
		return;
	}

	for (const auto &w : m_context->document->wires) {
		const auto wSide = wireSide(w->layer);
		if (!wSide.has_value() || w->points.isEmpty()) continue;
		if (m_netlist.netIdAt(w->points.first(), *wSide) != netId) continue;
		if (m_context->frontScene) {
			if (auto *item = m_context->frontScene->wireItemFor(w->uuid)) item->setNetHighlighted(true);
		}
		if (m_context->backScene) {
			if (auto *item = m_context->backScene->wireItemFor(w->uuid)) item->setNetHighlighted(true);
		}
	}
}

bool SelectTool::mousePress(BoardScene *scene, QGraphicsSceneMouseEvent *event) {
	if (event->button() != Qt::LeftButton) {
		return false;
	}

	QGraphicsItem *hit = scene->itemAt(event->scenePos(), QTransform());
	auto *placementItem = dynamic_cast<PlacementItem *>(hit);
	auto *wireItem = dynamic_cast<WireItem *>(hit);

	if (!placementItem && !wireItem) {
		if (!(event->modifiers() & Qt::ShiftModifier)) {
			scene->clearSelection();
		}
		return false;  // 空白部分のドラッグは Qt 標準のラバーバンド選択に任せる
	}

	const bool shift = event->modifiers() & Qt::ShiftModifier;
	QGraphicsItem *target = placementItem ? static_cast<QGraphicsItem *>(placementItem) : static_cast<QGraphicsItem *>(wireItem);

	if (shift) {
		target->setSelected(!target->isSelected());
	} else if (!target->isSelected()) {
		scene->clearSelection();
		target->setSelected(true);
	}
	// 既に選択済みの複数選択の1つをシフト無しでクリックした場合は、複数選択を維持
	// したままドラッグを開始できるようにする (よくある UX)。

	if (placementItem && target->isSelected()) {
		m_dragging = true;
		m_dragPressScenePos = event->scenePos();
		m_dragAnchorStart = placementItem->placement()->pos;
		m_lastDragDelta = QPoint(0, 0);
		m_dragEntries.clear();
		for (const QString &uuid : selectedPlacementUuids()) {
			const int idx = m_context->document->indexOfPlacement(uuid);
			if (idx >= 0) {
				m_dragEntries.append({uuid, m_context->document->placements[idx]->pos});
			}
		}
	}
	return true;
}

bool SelectTool::mouseMove(BoardScene *scene, QGraphicsSceneMouseEvent *event) {
	if (!m_dragging) {
		updateNetHighlight(scene, event);
		return false;
	}
	const QPointF rawDelta = event->scenePos() - m_dragPressScenePos;
	const QPoint proposedAnchor = SnapEngine::snapForPlacement(QPointF(m_dragAnchorStart) + rawDelta);
	m_lastDragDelta = proposedAnchor - m_dragAnchorStart;

	for (const auto &entry : m_dragEntries) {
		const QPoint newPos = entry.startPos + m_lastDragDelta;
		if (m_context->frontScene) {
			if (auto *item = m_context->frontScene->placementItemFor(entry.uuid)) item->setPos(newPos);
		}
		if (m_context->backScene) {
			if (auto *item = m_context->backScene->placementItemFor(entry.uuid)) item->setPos(newPos);
		}
	}
	return true;
}

bool SelectTool::mouseRelease(BoardScene *, QGraphicsSceneMouseEvent *event) {
	if (!m_dragging) {
		return false;
	}
	m_dragging = false;
	if (event->button() != Qt::LeftButton) {
		return true;
	}

	if (m_lastDragDelta != QPoint(0, 0)) {
		auto *stack = m_context->document->undoStack();
		const bool useMacro = m_dragEntries.size() > 1;
		if (useMacro) {
			stack->beginMacro(QObject::tr("複数部品の移動"));
		}
		for (const auto &entry : m_dragEntries) {
			stack->push(
				new MovePlacementCommand(m_context, entry.uuid, entry.startPos, entry.startPos + m_lastDragDelta));
		}
		if (useMacro) {
			stack->endMacro();
		}
	}
	m_dragEntries.clear();
	m_lastDragDelta = QPoint(0, 0);
	return true;
}

bool SelectTool::mouseDoubleClick(BoardScene *scene, QGraphicsSceneMouseEvent *event) {
	QGraphicsItem *hit = scene->itemAt(event->scenePos(), QTransform());
	auto *placementItem = dynamic_cast<PlacementItem *>(hit);
	if (!placementItem) {
		return false;
	}

	const auto placement = placementItem->placement();
	bool ok1 = false;
	const QString newRefDes = QInputDialog::getText(nullptr, QObject::tr("ラベル設定"), QObject::tr("Parts No"),
													 QLineEdit::Normal, placement->refDes, &ok1);
	if (!ok1) {
		return true;
	}
	bool ok2 = false;
	const QString newValue = QInputDialog::getText(nullptr, QObject::tr("ラベル設定"), QObject::tr("値/Name"),
													QLineEdit::Normal, placement->value, &ok2);
	if (!ok2) {
		return true;
	}
	if (newRefDes != placement->refDes || newValue != placement->value) {
		m_context->document->undoStack()->push(new SetPlacementLabelCommand(
			m_context, placement->uuid, placement->refDes, placement->value, newRefDes, newValue));
	}
	return true;
}

bool SelectTool::keyPress(BoardScene *, QKeyEvent *event) {
	if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
		deleteSelected();
		return true;
	}
	if (event->key() == Qt::Key_R) {
		rotateSelected();
		return true;
	}
	if (event->key() == Qt::Key_F) {
		flipSelected();
		return true;
	}
	if (event->modifiers() & Qt::ControlModifier) {
		if (event->key() == Qt::Key_C) {
			copySelected();
			return true;
		}
		if (event->key() == Qt::Key_V) {
			pasteWithOffset(QPoint(units::Pitch * 2, units::Pitch * 2));
			return true;
		}
	}
	QPoint delta(0, 0);
	bool isArrow = true;
	switch (event->key()) {
	case Qt::Key_Left:
		delta = QPoint(-units::Pitch, 0);
		break;
	case Qt::Key_Right:
		delta = QPoint(units::Pitch, 0);
		break;
	case Qt::Key_Up:
		delta = QPoint(0, -units::Pitch);
		break;
	case Qt::Key_Down:
		delta = QPoint(0, units::Pitch);
		break;
	default:
		isArrow = false;
		break;
	}
	if (isArrow) {
		nudgeSelected(delta);
		return true;
	}
	return false;
}

bool SelectTool::contextMenu(BoardScene *scene, QGraphicsSceneContextMenuEvent *event) {
	QGraphicsItem *hit = scene->itemAt(event->scenePos(), QTransform());
	auto *placementItem = dynamic_cast<PlacementItem *>(hit);
	auto *wireItem = dynamic_cast<WireItem *>(hit);

	if (placementItem || wireItem) {
		QGraphicsItem *target =
			placementItem ? static_cast<QGraphicsItem *>(placementItem) : static_cast<QGraphicsItem *>(wireItem);
		if (!target->isSelected()) {
			scene->clearSelection();
			target->setSelected(true);
		}
		QMenu menu;
		QAction *rotateAction = placementItem ? menu.addAction(QObject::tr("回転 (+90°)")) : nullptr;
		QAction *flipAction = placementItem ? menu.addAction(QObject::tr("表裏切替")) : nullptr;
		QAction *copyAction = menu.addAction(QObject::tr("コピー"));
		QAction *deleteAction = menu.addAction(QObject::tr("削除"));
		QAction *chosen = menu.exec(event->screenPos());
		if (chosen && chosen == rotateAction) {
			rotateSelected();
		} else if (chosen && chosen == flipAction) {
			flipSelected();
		} else if (chosen == copyAction) {
			copySelected();
		} else if (chosen == deleteAction) {
			deleteSelected();
		}
		return true;
	}

	if (!m_context->clipboardPlacements.isEmpty() || !m_context->clipboardWires.isEmpty()) {
		QMenu menu;
		QAction *pasteAction = menu.addAction(QObject::tr("貼り付け"));
		QAction *chosen = menu.exec(event->screenPos());
		if (chosen == pasteAction) {
			pasteAt(event->scenePos());
		}
		return true;
	}
	return false;
}

void SelectTool::rotateSelected() {
	const auto uuids = selectedPlacementUuids();
	if (uuids.isEmpty()) {
		return;
	}
	auto *stack = m_context->document->undoStack();
	const bool useMacro = uuids.size() > 1;
	if (useMacro) stack->beginMacro(QObject::tr("選択部品の回転"));
	for (const auto &uuid : uuids) {
		const int idx = m_context->document->indexOfPlacement(uuid);
		if (idx < 0) continue;
		const Rotation oldRot = m_context->document->placements[idx]->rot;
		stack->push(new RotatePlacementCommand(m_context, uuid, oldRot, rotateCW(oldRot)));
	}
	if (useMacro) stack->endMacro();
}

void SelectTool::flipSelected() {
	const auto uuids = selectedPlacementUuids();
	if (uuids.isEmpty()) {
		return;
	}
	auto *stack = m_context->document->undoStack();
	const bool useMacro = uuids.size() > 1;
	if (useMacro) stack->beginMacro(QObject::tr("選択部品の表裏切替"));
	for (const auto &uuid : uuids) {
		const int idx = m_context->document->indexOfPlacement(uuid);
		if (idx < 0) continue;
		const Side oldSide = m_context->document->placements[idx]->side;
		const Side newSide = (oldSide == Side::Front) ? Side::Back : Side::Front;
		stack->push(new FlipPlacementSideCommand(m_context, uuid, oldSide, newSide));
	}
	if (useMacro) stack->endMacro();
}

void SelectTool::deleteSelected() {
	const auto pUuids = selectedPlacementUuids();
	const auto wUuids = selectedWireUuids();
	if (pUuids.isEmpty() && wUuids.isEmpty()) {
		return;
	}
	auto *stack = m_context->document->undoStack();
	stack->beginMacro(QObject::tr("選択範囲の削除"));
	for (const auto &uuid : pUuids) stack->push(new RemovePlacementCommand(m_context, uuid));
	for (const auto &uuid : wUuids) stack->push(new RemoveWireCommand(m_context, uuid));
	stack->endMacro();
}

void SelectTool::nudgeSelected(QPoint delta) {
	const auto uuids = selectedPlacementUuids();
	if (uuids.isEmpty()) {
		return;
	}
	auto *stack = m_context->document->undoStack();
	const bool useMacro = uuids.size() > 1;
	if (useMacro) stack->beginMacro(QObject::tr("選択部品の移動"));
	for (const auto &uuid : uuids) {
		const int idx = m_context->document->indexOfPlacement(uuid);
		if (idx < 0) continue;
		const QPoint oldPos = m_context->document->placements[idx]->pos;
		stack->push(new MovePlacementCommand(m_context, uuid, oldPos, oldPos + delta));
	}
	if (useMacro) stack->endMacro();
}

void SelectTool::copySelected() {
	m_context->clipboardPlacements.clear();
	m_context->clipboardWires.clear();
	for (const QString &uuid : selectedPlacementUuids()) {
		const int idx = m_context->document->indexOfPlacement(uuid);
		if (idx >= 0) {
			m_context->clipboardPlacements.append(*m_context->document->placements[idx]);
		}
	}
	for (const QString &uuid : selectedWireUuids()) {
		const int idx = m_context->document->indexOfWire(uuid);
		if (idx >= 0) {
			m_context->clipboardWires.append(*m_context->document->wires[idx]);
		}
	}
}

void SelectTool::pasteWithOffset(QPoint offset) {
	if (m_context->clipboardPlacements.isEmpty() && m_context->clipboardWires.isEmpty()) {
		return;
	}
	auto *stack = m_context->document->undoStack();
	stack->beginMacro(QObject::tr("貼り付け"));

	QVector<QString> newPlacementUuids;
	QVector<QString> newWireUuids;
	for (const Placement &src : m_context->clipboardPlacements) {
		auto p = std::make_shared<Placement>(src);
		p->uuid = ids::newUuid();
		p->pos += offset;
		p->z = m_context->document->nextZValue();
		stack->push(new AddPlacementCommand(m_context, p));
		newPlacementUuids.append(p->uuid);
	}
	for (const Wire &src : m_context->clipboardWires) {
		auto w = std::make_shared<Wire>(src);
		w->uuid = ids::newUuid();
		for (auto &pt : w->points) pt += offset;
		stack->push(new AddWireCommand(m_context, w));
		newWireUuids.append(w->uuid);
	}
	stack->endMacro();

	if (m_context->frontScene) {
		m_context->frontScene->clearSelection();
		for (const auto &uuid : newPlacementUuids) {
			if (auto *item = m_context->frontScene->placementItemFor(uuid)) item->setSelected(true);
		}
		for (const auto &uuid : newWireUuids) {
			if (auto *item = m_context->frontScene->wireItemFor(uuid)) item->setSelected(true);
		}
	}
}

void SelectTool::pasteAt(QPointF scenePos) {
	if (m_context->clipboardPlacements.isEmpty() && m_context->clipboardWires.isEmpty()) {
		return;
	}
	QPoint minPt(INT_MAX, INT_MAX);
	for (const auto &p : m_context->clipboardPlacements) {
		minPt.setX(qMin(minPt.x(), p.pos.x()));
		minPt.setY(qMin(minPt.y(), p.pos.y()));
	}
	for (const auto &w : m_context->clipboardWires) {
		for (const auto &pt : w.points) {
			minPt.setX(qMin(minPt.x(), pt.x()));
			minPt.setY(qMin(minPt.y(), pt.y()));
		}
	}
	if (minPt.x() == INT_MAX) {
		return;
	}
	const QPoint target = SnapEngine::snapForPlacement(scenePos);
	pasteWithOffset(target - minPt);
}

QString SelectTool::statusHint() const {
	return QObject::tr("クリックで選択 / ドラッグで移動 / R:回転 F:表裏切替 Delete:削除 右クリック:メニュー");
}
