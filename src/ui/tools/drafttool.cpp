#include "drafttool.h"

#include <QGraphicsSceneMouseEvent>

#include "../../render/boardscene.h"
#include "../../render/items/overlayitem.h"

void DraftTool::deactivate() {
	if (m_activeScene) {
		m_activeScene->overlay()->clearLiveDraftStroke();
	}
	m_activeScene = nullptr;
	m_currentStroke.clear();
}

bool DraftTool::mousePress(BoardScene *scene, QGraphicsSceneMouseEvent *event) {
	if (event->button() != Qt::LeftButton) {
		return false;
	}
	m_activeScene = scene;
	m_currentStroke = {event->scenePos()};
	scene->overlay()->setLiveDraftStroke(m_currentStroke);
	return true;
}

bool DraftTool::mouseMove(BoardScene *scene, QGraphicsSceneMouseEvent *event) {
	if (!m_activeScene || scene != m_activeScene || m_currentStroke.isEmpty()) {
		return false;
	}
	m_currentStroke.append(event->scenePos());
	scene->overlay()->setLiveDraftStroke(m_currentStroke);
	return true;
}

bool DraftTool::mouseRelease(BoardScene *scene, QGraphicsSceneMouseEvent *event) {
	if (!m_activeScene || scene != m_activeScene) {
		return false;
	}
	if (event->button() == Qt::LeftButton) {
		scene->overlay()->addDraftStroke(m_currentStroke);
		scene->overlay()->clearLiveDraftStroke();
		m_currentStroke.clear();
		m_activeScene = nullptr;
		return true;
	}
	return false;
}

QString DraftTool::statusHint() const {
	return QObject::tr("ドラッグでフリーハンドの下書き (保存されません)");
}
