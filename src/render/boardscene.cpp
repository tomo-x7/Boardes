#include "boardscene.h"

#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QSet>

#include "../model/librarymanager.h"
#include "../ui/tools/toolmanager.h"
#include "items/boarditem.h"
#include "items/labelitem.h"
#include "items/overlayitem.h"
#include "items/placementitem.h"
#include "items/wireitem.h"

namespace {

// 全アイテムの親となる、何も描画しない透明なコンテナ。裏面シーンではこれ自体に
// 水平反転をかけることで配下のアイテムをまとめて鏡像化する。
class SceneRootItem : public QGraphicsItem {
public:
	QRectF boundingRect() const override {
		return QRectF();
	}
	void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *) override {
	}
};

}  // namespace

BoardScene::BoardScene(Side side, QObject *parent) : QGraphicsScene(parent), m_side(side) {
	m_root = new SceneRootItem();
	addItem(m_root);
	m_overlay = new OverlayItem(m_root);
}

BoardScene::~BoardScene() = default;

void BoardScene::clearAllItems() {
	qDeleteAll(m_placementItems);
	m_placementItems.clear();
	qDeleteAll(m_wireItems);
	m_wireItems.clear();
	qDeleteAll(m_labelItems);
	m_labelItems.clear();
	delete m_boardItem;
	m_boardItem = nullptr;
}

void BoardScene::setDocument(Document *document, LibraryManager *libraryManager) {
	clearAllItems();
	m_document = document;
	m_libraryManager = libraryManager;
	syncBoard();
	syncPlacements();
	syncWires();
}

namespace {
QTransform backRootTransform(BackViewMode mode, QSize boardSize) {
	switch (mode) {
	case BackViewMode::Through:
		return QTransform();
	case BackViewMode::MirrorY:
		return QTransform(1, 0, 0, -1, 0, boardSize.height());
	case BackViewMode::MirrorX:
	default:
		return QTransform(-1, 0, 0, 1, boardSize.width(), 0);
	}
}
}  // namespace

void BoardScene::syncBoard() {
	const BoardSpec *board = m_document ? &m_document->board : nullptr;
	if (!m_boardItem) {
		m_boardItem = new BoardItem(board, m_side, m_root);
	} else {
		m_boardItem->setBoard(board);
	}
	if (board) {
		m_root->setTransform(m_side == Side::Back ? backRootTransform(m_backViewMode, board->size) : QTransform());
		m_boardRect = QRectF(QPointF(0, 0), QSizeF(board->size));
		// スクロール範囲は基板ぴったりだとパンできない (基板が画面に収まった時点で
		// スクロールバーの可動域が0になる) ので、周囲に余白を持たせる (Phase 12)。
		const qreal margin = qMax(200.0, qMax(board->size.width(), board->size.height()) * 0.5);
		setSceneRect(m_boardRect.adjusted(-margin, -margin, margin, margin));
	}
}

void BoardScene::setBackViewMode(BackViewMode mode) {
	if (m_backViewMode == mode) {
		return;
	}
	m_backViewMode = mode;
	syncBoard();
	syncLabels();
	// ピン番号の反転もこのシーンの反転モードに依存するため反映し直す。
	for (auto *item : std::as_const(m_placementItems)) {
		item->setTextFlip(textFlipX(), textFlipY());
	}
}

bool BoardScene::textFlipX() const {
	if (m_side != Side::Back) {
		return false;
	}
	return m_backViewMode == BackViewMode::MirrorX;
}

bool BoardScene::textFlipY() const {
	if (m_side != Side::Back) {
		return false;
	}
	return m_backViewMode == BackViewMode::MirrorY;
}

void BoardScene::syncPlacements() {
	if (!m_document) {
		return;
	}
	QSet<QString> currentUuids;
	for (const auto &p : m_document->placements) {
		currentUuids.insert(p->uuid);
		auto it = m_placementItems.find(p->uuid);
		if (it == m_placementItems.end()) {
			auto *item = new PlacementItem(p, m_libraryManager, m_side, m_root);
			item->setShowPinNumbers(m_showPinNumbers);
			item->setForceOutline(m_forceOutline);
			item->setTextFlip(textFlipX(), textFlipY());
			item->setPinMarkers(m_pinMarkersVisible, m_pinMarkerColor, m_pinMarkerDiameter);
			m_placementItems.insert(p->uuid, item);
		} else {
			it.value()->refresh();
		}
	}
	for (auto it = m_placementItems.begin(); it != m_placementItems.end();) {
		if (!currentUuids.contains(it.key())) {
			delete it.value();
			it = m_placementItems.erase(it);
		} else {
			++it;
		}
	}
	syncLabels();
}

void BoardScene::syncLabels() {
	if (!m_document) {
		return;
	}
	QSet<QString> currentUuids;
	for (const auto &p : m_document->placements) {
		currentUuids.insert(p->uuid);
		auto it = m_labelItems.find(p->uuid);
		LabelItem *item;
		if (it == m_labelItems.end()) {
			item = new LabelItem(m_root);
			m_labelItems.insert(p->uuid, item);
		} else {
			item = it.value();
		}
		item->setCounterMirror(textFlipX(), textFlipY());
		item->setTexts(p->refDes, p->value);

		QSize sz(20, 20);
		if (m_libraryManager) {
			if (const auto part = m_libraryManager->resolvePart(p->libraryId, p->partId)) {
				sz = resolvedBoundingSize(part->size(), p->rot);
			}
		}
		item->setAnchor(QPointF(p->pos.x(), p->pos.y() + sz.height() + 1));
		item->setVisible(p->labelsVisible && m_showLabels);
	}
	for (auto it = m_labelItems.begin(); it != m_labelItems.end();) {
		if (!currentUuids.contains(it.key())) {
			delete it.value();
			it = m_labelItems.erase(it);
		} else {
			++it;
		}
	}
}

void BoardScene::syncWires() {
	if (!m_document) {
		return;
	}
	QSet<QString> currentUuids;
	for (const auto &w : m_document->wires) {
		const auto side = wireSide(w->layer);
		const bool belongsHere = !side.has_value() || *side == m_side;
		if (!belongsHere) {
			continue;
		}
		currentUuids.insert(w->uuid);
		auto it = m_wireItems.find(w->uuid);
		// 実際の表示可否は「レイヤ表示トグル」と「オブジェクト一覧の目玉トグル
		// (w->visible)」の両方の AND。
		const bool combinedVisible = m_layerVisible.value(static_cast<int>(w->layer), true) && w->visible;
		if (it == m_wireItems.end()) {
			auto *item = new WireItem(w, m_root);
			item->setVisible(combinedVisible);
			m_wireItems.insert(w->uuid, item);
		} else {
			it.value()->refresh();
			it.value()->setVisible(combinedVisible);
		}
	}
	for (auto it = m_wireItems.begin(); it != m_wireItems.end();) {
		if (!currentUuids.contains(it.key())) {
			delete it.value();
			it = m_wireItems.erase(it);
		} else {
			++it;
		}
	}
}

void BoardScene::setShowPinNumbers(bool show) {
	m_showPinNumbers = show;
	for (auto *item : std::as_const(m_placementItems)) {
		item->setShowPinNumbers(show);
	}
}

void BoardScene::setShowLabels(bool show) {
	m_showLabels = show;
	syncLabels();
}

void BoardScene::setForcePartOutline(bool onlyOutline) {
	m_forceOutline = onlyOutline;
	for (auto *item : std::as_const(m_placementItems)) {
		item->setForceOutline(onlyOutline);
	}
}

void BoardScene::setLayerVisible(WireLayer layer, bool visible) {
	m_layerVisible[static_cast<int>(layer)] = visible;
	for (auto it = m_wireItems.constBegin(); it != m_wireItems.constEnd(); ++it) {
		if (it.value()->wire()->layer == layer) {
			// オブジェクト一覧の目玉トグルとの AND (Phase 15)。
			it.value()->setVisible(visible && it.value()->wire()->visible);
		}
	}
}

bool BoardScene::isLayerVisible(WireLayer layer) const {
	return m_layerVisible.value(static_cast<int>(layer), true);
}

void BoardScene::setPinMarkers(bool visible, QColor color, qreal diameterUnits) {
	m_pinMarkersVisible = visible;
	m_pinMarkerColor = color;
	m_pinMarkerDiameter = diameterUnits;
	for (auto *item : std::as_const(m_placementItems)) {
		item->setPinMarkers(visible, color, diameterUnits);
	}
}

// ツールが消費した場合は明示的に accept() する (BoardView 側や親ウィジェットへの
// 意図しないイベント伝播 - 例えば Escape がウィンドウのショートカットとして
// 二重に解釈される、といったことを防ぐ)。

void BoardScene::mousePressEvent(QGraphicsSceneMouseEvent *event) {
	if (m_toolManager && m_toolManager->handleMousePress(this, event)) {
		event->accept();
		return;
	}
	QGraphicsScene::mousePressEvent(event);
}

void BoardScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
	if (m_toolManager && m_toolManager->handleMouseMove(this, event)) {
		event->accept();
		return;
	}
	QGraphicsScene::mouseMoveEvent(event);
}

void BoardScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
	if (m_toolManager && m_toolManager->handleMouseRelease(this, event)) {
		event->accept();
		return;
	}
	QGraphicsScene::mouseReleaseEvent(event);
}

void BoardScene::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) {
	if (m_toolManager && m_toolManager->handleMouseDoubleClick(this, event)) {
		event->accept();
		return;
	}
	QGraphicsScene::mouseDoubleClickEvent(event);
}

void BoardScene::keyPressEvent(QKeyEvent *event) {
	if (m_toolManager && m_toolManager->handleKeyPress(this, event)) {
		event->accept();
		return;
	}
	QGraphicsScene::keyPressEvent(event);
}

void BoardScene::contextMenuEvent(QGraphicsSceneContextMenuEvent *event) {
	if (m_toolManager && m_toolManager->handleContextMenu(this, event)) {
		event->accept();
		return;
	}
	QGraphicsScene::contextMenuEvent(event);
}
