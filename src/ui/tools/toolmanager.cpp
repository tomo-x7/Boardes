#include "toolmanager.h"

#include "drafttool.h"
#include "placeparttool.h"
#include "selecttool.h"
#include "tool.h"
#include "wiretool.h"

ToolManager::ToolManager(ToolContext context, QObject *parent) : QObject(parent), m_context(std::move(context)) {
	activateSelectTool();
}

ToolManager::~ToolManager() = default;

void ToolManager::setTool(std::unique_ptr<Tool> tool) {
	if (m_activeTool) {
		m_activeTool->deactivate();
	}
	m_activeTool = std::move(tool);
	if (m_activeTool) {
		m_activeTool->activate();
	}
	emit activeToolChanged();
	emit statusHintChanged(m_activeTool ? m_activeTool->statusHint() : QString());
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

void ToolManager::activateWireTool(WireLayer layer) {
	setTool(std::make_unique<WireTool>(&m_context, layer));
}

void ToolManager::activateDraftTool() {
	setTool(std::make_unique<DraftTool>(&m_context));
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
