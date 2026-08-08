#include "wiretool.h"

#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>

#include "../../commands/wirecommands.h"
#include "../../core/ids.h"
#include "../../model/document.h"
#include "../../render/boardscene.h"
#include "../../render/items/overlayitem.h"
#include "../input/keymap.h"
#include "snapengine.h"

WireTool::WireTool(ToolContext *context, WireKind kind) : Tool(context), m_kind(kind) {
}

void WireTool::deactivate() {
	finish(false);
}

QPoint WireTool::snapNext(QPointF modelPos) const {
	if (m_points.isEmpty()) {
		return m_context->snapEngine->snapForWire(modelPos);
	}
	return m_context->snapEngine->snapForWireVertex(m_points.last(), modelPos);
}

bool WireTool::mousePress(BoardScene *scene, QGraphicsSceneMouseEvent *event) {
	if (m_context->keymapOrDefault().matchesMouseButton(QStringLiteral("wire.commit"), event->button(),
														 event->modifiers(), InputKind::MouseButton)) {
		if (m_activeScene) {
			finish(true);
			return true;
		}
		return false;
	}
	if (event->button() != Qt::LeftButton) {
		return false;
	}

	const QPoint snapped = snapNext(scene->toModel(event->scenePos()));
	if (!m_activeScene) {
		m_activeScene = scene;
		m_points = {snapped};
	} else if (scene == m_activeScene) {
		m_points.append(snapped);
	} else {
		return true;  // 描画開始時と別の面でのクリックは無視
	}
	scene->overlay()->setWirePreview(m_points, snapped, wireLayerFor(m_kind, scene->side()));
	return true;
}

bool WireTool::mouseMove(BoardScene *scene, QGraphicsSceneMouseEvent *event) {
	if (!m_activeScene || scene != m_activeScene) {
		return false;
	}
	const QPoint snapped = snapNext(scene->toModel(event->scenePos()));
	scene->overlay()->setWirePreview(m_points, snapped, wireLayerFor(m_kind, scene->side()));
	return true;
}

bool WireTool::mouseRelease(BoardScene *scene, QGraphicsSceneMouseEvent *) {
	// press 側で処理は完結しているが、対になる release も消費したことにして
	// Qt 側のデフォルト処理 (ラバーバンド選択の開始判定など) に渡さないようにする。
	return m_activeScene == scene;
}

bool WireTool::keyPress(BoardScene *, QKeyEvent *event) {
	const Keymap &keymap = m_context->keymapOrDefault();
	if (keymap.matchesKey(QStringLiteral("wire.discard"), event) && m_activeScene) {
		finish(false);
		return true;
	}
	if (keymap.matchesKey(QStringLiteral("wire.commit"), event) && m_activeScene) {
		finish(true);
		return true;
	}
	return false;
}

bool WireTool::cancel() {
	if (!m_activeScene) {
		return false;
	}
	finish(false);
	return true;
}

void WireTool::finish(bool commit) {
	if (commit && m_points.size() >= 2 && m_activeScene) {
		auto wire = std::make_shared<Wire>();
		wire->uuid = ids::newUuid();
		wire->layer = wireLayerFor(m_kind, m_activeScene->side());
		wire->points = m_points;
		m_context->document->undoStack()->push(new AddWireCommand(m_context, wire));
	}
	if (m_activeScene) {
		m_activeScene->overlay()->clearWirePreview();
	}
	m_points.clear();
	m_activeScene = nullptr;
}

QString WireTool::statusHint(const Keymap &keymap) const {
	return QObject::tr("クリックで頂点を追加 / %1で確定 / %2で破棄")
		.arg(keymap.displayFor(QStringLiteral("wire.commit")), keymap.displayFor(QStringLiteral("wire.discard")));
}
