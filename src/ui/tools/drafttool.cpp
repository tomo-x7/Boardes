#include "drafttool.h"

#include <QGraphicsSceneMouseEvent>

#include "../../render/boardscene.h"
#include "../../render/items/overlayitem.h"
#include "../input/keymap.h"

void DraftTool::deactivate() {
	if (m_activeScene) {
		m_activeScene->overlay()->clearLiveDraftStroke();
	}
	m_activeScene = nullptr;
	m_strokeButton = Qt::NoButton;
	m_currentStroke.clear();
}

bool DraftTool::mousePress(BoardScene *scene, QGraphicsSceneMouseEvent *event) {
	if (!m_context->keymapOrDefault().matchesMouseButton(QStringLiteral("draft.stroke"), event->button(),
														  event->modifiers(), InputKind::MouseDrag)) {
		return false;
	}
	m_activeScene = scene;
	m_strokeButton = event->button();
	m_currentStroke = {scene->toModel(event->scenePos())};
	scene->overlay()->setLiveDraftStroke(m_currentStroke);
	return true;
}

bool DraftTool::mouseMove(BoardScene *scene, QGraphicsSceneMouseEvent *event) {
	if (!m_activeScene || scene != m_activeScene || m_currentStroke.isEmpty()) {
		return false;
	}
	// ストロークを始めたボタンが押されていない移動は無視する。ボタンを離したのに release が
	// 届かなかった場合に、ボタンを押さないカーソル移動でストロークが伸び続けないようにする。
	if (!(event->buttons() & m_strokeButton)) {
		return false;
	}
	m_currentStroke.append(scene->toModel(event->scenePos()));
	scene->overlay()->setLiveDraftStroke(m_currentStroke);
	return true;
}

bool DraftTool::mouseRelease(BoardScene *scene, QGraphicsSceneMouseEvent *event) {
	if (!m_activeScene || scene != m_activeScene) {
		return false;
	}
	// ストロークを始めたボタンを離したら確定する。離した時点の修飾キーまで照合し直すと、
	// ストロークの途中で Shift 等を押しただけで確定できず、ボタンを離した後もストロークが
	// 生きたままカーソルに追従し続けてしまう。
	if (event->button() == m_strokeButton) {
		scene->overlay()->addDraftStroke(m_currentStroke);
		scene->overlay()->clearLiveDraftStroke();
		m_currentStroke.clear();
		m_activeScene = nullptr;
		m_strokeButton = Qt::NoButton;
		return true;
	}
	return false;
}

bool DraftTool::cancel() {
	if (m_currentStroke.isEmpty() && !m_activeScene) {
		return false;
	}
	if (m_activeScene) {
		m_activeScene->overlay()->clearLiveDraftStroke();
	}
	m_activeScene = nullptr;
	m_strokeButton = Qt::NoButton;
	m_currentStroke.clear();
	return true;
}

QString DraftTool::statusHint(const Keymap &keymap) const {
	return QObject::tr("%1でフリーハンドの下書き (保存されません)").arg(keymap.displayFor(QStringLiteral("draft.stroke")));
}
