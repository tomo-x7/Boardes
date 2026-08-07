#include "wiretool.h"

#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>

#include "../../commands/wirecommands.h"
#include "../../core/ids.h"
#include "../../model/document.h"
#include "../../render/boardscene.h"
#include "../../render/items/overlayitem.h"
#include "snapengine.h"

WireTool::WireTool(ToolContext *context, WireLayer layer) : Tool(context), m_layer(layer) {
}

void WireTool::deactivate() {
	finish(false);
}

QPoint WireTool::snapNext(QPointF scenePos) const {
	if (m_points.isEmpty()) {
		return m_context->snapEngine->snapForWire(scenePos);
	}
	return m_context->snapEngine->snapForWireVertex(m_points.last(), scenePos);
}

bool WireTool::mousePress(BoardScene *scene, QGraphicsSceneMouseEvent *event) {
	if (event->button() == Qt::RightButton) {
		if (m_activeScene) {
			finish(true);
			return true;
		}
		return false;
	}
	if (event->button() != Qt::LeftButton) {
		return false;
	}

	const QPoint snapped = snapNext(event->scenePos());
	if (!m_activeScene) {
		m_activeScene = scene;
		m_points = {snapped};
	} else if (scene == m_activeScene) {
		m_points.append(snapped);
	} else {
		return true;  // 描画開始時と別の面でのクリックは無視
	}
	scene->overlay()->setWirePreview(m_points, snapped, m_layer);
	return true;
}

bool WireTool::mouseMove(BoardScene *scene, QGraphicsSceneMouseEvent *event) {
	if (!m_activeScene || scene != m_activeScene) {
		return false;
	}
	const QPoint snapped = snapNext(event->scenePos());
	scene->overlay()->setWirePreview(m_points, snapped, m_layer);
	return true;
}

bool WireTool::mouseRelease(BoardScene *scene, QGraphicsSceneMouseEvent *) {
	// press 側で処理は完結しているが、対になる release も消費したことにして
	// Qt 側のデフォルト処理 (ラバーバンド選択の開始判定など) に渡さないようにする。
	return m_activeScene == scene;
}

bool WireTool::mouseDoubleClick(BoardScene *scene, QGraphicsSceneMouseEvent *event) {
	if (m_activeScene && scene == m_activeScene) {
		// ダブルクリック自体の位置も最後の頂点として数える (先行する press で既に
		// 同じ位置が追加済みなら二重には追加しない)。
		const QPoint snapped = snapNext(event->scenePos());
		if (m_points.isEmpty() || m_points.last() != snapped) {
			m_points.append(snapped);
		}
		finish(true);
		return true;
	}
	return false;
}

bool WireTool::keyPress(BoardScene *, QKeyEvent *event) {
	if (event->key() == Qt::Key_Escape && m_activeScene) {
		finish(false);
		return true;
	}
	return false;
}

void WireTool::finish(bool commit) {
	if (commit && m_points.size() >= 2 && m_activeScene) {
		auto wire = std::make_shared<Wire>();
		wire->uuid = ids::newUuid();
		wire->layer = m_layer;
		wire->points = m_points;
		m_context->document->undoStack()->push(new AddWireCommand(m_context, wire));
	}
	if (m_activeScene) {
		m_activeScene->overlay()->clearWirePreview();
	}
	m_points.clear();
	m_activeScene = nullptr;
}

QString WireTool::statusHint() const {
	return QObject::tr("クリックで頂点を追加 / 右クリックかダブルクリックで確定 / Esc で破棄");
}
