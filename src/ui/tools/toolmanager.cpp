#include "toolmanager.h"

#include "../input/keymap.h"
#include "drafttool.h"
#include "placeparttool.h"
#include "selecttool.h"
#include "tool.h"
#include "wiretool.h"

ToolManager::ToolManager(ToolContext context, QObject *parent) : QObject(parent), m_context(std::move(context)) {
	m_context.toolManager = this;
	if (m_context.keymap) {
		// カスタマイズが変わったら、今表示中のステータスバーのヒントを新しい割り当てで
		// 再表示する (GIMP 風のリアルタイムヒントがカスタム設定を反映するようにするため)。
		connect(m_context.keymap, &Keymap::changed, this, [this] {
			emit statusHintChanged(m_activeTool ? m_activeTool->statusHint(m_context.keymapOrDefault()) : QString());
		});
	}
	activateSelectTool();
}

ToolManager::~ToolManager() = default;

void ToolManager::setTool(std::unique_ptr<Tool> tool) {
	const bool hadPendingPart = !m_context.pendingPartId.isEmpty();
	if (m_activeTool) {
		m_activeTool->deactivate();
	}
	m_activeTool = std::move(tool);
	// 配置ツール以外に切り替わるときは、パレット側の「配置中」表示もクリアする。
	if (!dynamic_cast<PlacePartTool *>(m_activeTool.get())) {
		m_context.pendingPartId.clear();
		m_context.pendingLibraryId.clear();
		if (hadPendingPart) {
			emit pendingPartCleared();
		}
	}
	if (auto *st = dynamic_cast<SelectTool *>(m_activeTool.get())) {
		connect(st, &SelectTool::selectionSummaryChanged, this, &ToolManager::selectionSummaryChanged);
	} else {
		emit selectionSummaryChanged(QString());
	}
	if (m_activeTool) {
		m_activeTool->activate();
	}
	emit activeToolChanged();
	emit statusHintChanged(m_activeTool ? m_activeTool->statusHint(m_context.keymapOrDefault()) : QString());
}

void ToolManager::activateSelectTool() {
	setTool(std::make_unique<SelectTool>(&m_context));
}

void ToolManager::activatePlacePartTool(const QString &libraryId, const QString &partId) {
	m_context.pendingLibraryId = libraryId;
	m_context.pendingPartId = partId;
	m_context.pendingRotation = Rotation::R0;
	setTool(std::make_unique<PlacePartTool>(&m_context));
}

void ToolManager::activateWireTool(WireKind kind) {
	setTool(std::make_unique<WireTool>(&m_context, kind));
}

void ToolManager::activateDraftTool() {
	setTool(std::make_unique<DraftTool>(&m_context));
}

void ToolManager::cancelCurrent() {
	if (m_activeTool && m_activeTool->cancel()) {
		return;
	}
	activateSelectTool();
}

bool ToolManager::handleMousePress(BoardScene *scene, QGraphicsSceneMouseEvent *event) {
	return m_activeTool && m_activeTool->mousePress(scene, event);
}
bool ToolManager::handleMouseMove(BoardScene *scene, QGraphicsSceneMouseEvent *event) {
	return m_activeTool && m_activeTool->mouseMove(scene, event);
}
bool ToolManager::handleMouseRelease(BoardScene *scene, QGraphicsSceneMouseEvent *event) {
	return m_activeTool && m_activeTool->mouseRelease(scene, event);
}
bool ToolManager::handleMouseDoubleClick(BoardScene *scene, QGraphicsSceneMouseEvent *event) {
	return m_activeTool && m_activeTool->mouseDoubleClick(scene, event);
}
bool ToolManager::handleKeyPress(BoardScene *scene, QKeyEvent *event) {
	return m_activeTool && m_activeTool->keyPress(scene, event);
}
bool ToolManager::handleContextMenu(BoardScene *scene, QGraphicsSceneContextMenuEvent *event) {
	return m_activeTool && m_activeTool->contextMenu(scene, event);
}
void ToolManager::handleMouseLeave(BoardScene *scene) {
	if (m_activeTool) {
		m_activeTool->mouseLeave(scene);
	}
}
