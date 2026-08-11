#include "boardview.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <QWheelEvent>
#include <cmath>

#include "boardscene.h"

namespace {
constexpr qreal kZoomStep = 1.15;
constexpr qreal kMinZoom = 0.1;
constexpr qreal kMaxZoom = 32.0;
}  // namespace

BoardView::BoardView(QWidget *parent) : QGraphicsView(parent) {
	setRenderHint(QPainter::Antialiasing, true);
	setRenderHint(QPainter::SmoothPixmapTransform, true);
	// ボタンを押していないマウス移動でも mouseMoveEvent を受け取れるようにする
	// (SelectTool の「ホバーで同一ネットをハイライト」に必要)。
	setMouseTracking(true);
	viewport()->setMouseTracking(true);
	setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
	setResizeAnchor(QGraphicsView::AnchorViewCenter);
	setDragMode(QGraphicsView::RubberBandDrag);
	setFrameShape(QFrame::NoFrame);
	setBackgroundBrush(QColor(60, 60, 60));
	m_dragModeBeforeSpace = dragMode();
	// Space 押下中のパン (ScrollHandDrag への切替) にはビューがフォーカスを持てる
	// 必要がある。
	setFocusPolicy(Qt::StrongFocus);
}

QPointF BoardView::viewCenterModel() const {
	auto *bs = qobject_cast<BoardScene *>(scene());
	if (!bs) {
		return QPointF();
	}
	return bs->toModel(mapToScene(viewport()->rect().center()));
}

void BoardView::centerOnModel(QPointF modelPos) {
	auto *bs = qobject_cast<BoardScene *>(scene());
	if (!bs) {
		return;
	}
	centerOn(bs->fromModel(modelPos));
}

void BoardView::setZoom(qreal factor) {
	factor = qBound(kMinZoom, factor, kMaxZoom);
	if (qFuzzyCompare(factor, m_zoom)) {
		return;
	}
	const qreal ratio = factor / m_zoom;
	scale(ratio, ratio);
	m_zoom = factor;
	emit zoomChanged(m_zoom);
}

void BoardView::zoomIn() {
	setZoom(m_zoom * kZoomStep);
}

void BoardView::zoomOut() {
	setZoom(m_zoom / kZoomStep);
}

void BoardView::resetZoom() {
	setZoom(1.0);
}

void BoardView::fitBoardToWindow() {
	if (!scene()) {
		return;
	}
	auto *bs = qobject_cast<BoardScene *>(scene());
	const QRectF target = (bs && !bs->boardRect().isEmpty()) ? bs->boardRect() : sceneRect();
	if (target.isEmpty()) {
		return;
	}
	fitInView(target, Qt::KeepAspectRatio);
	m_zoom = transform().m11();
	emit zoomChanged(m_zoom);
}

void BoardView::wheelEvent(QWheelEvent *event) {
	if (event->angleDelta().y() > 0) {
		zoomIn();
	} else if (event->angleDelta().y() < 0) {
		zoomOut();
	}
	event->accept();
}

void BoardView::beginPan(const QPoint &pos) {
	m_panning = true;
	m_lastPanPos = pos;
	setCursor(Qt::ClosedHandCursor);
}

void BoardView::mousePressEvent(QMouseEvent *event) {
	if (keymapOrDefault().matchesMouseButton(QStringLiteral("view.pan"), event->button(), event->modifiers(),
											 InputKind::MouseDrag)) {
		beginPan(event->pos());
		event->accept();
		return;
	}
	QGraphicsView::mousePressEvent(event);
}

void BoardView::mouseMoveEvent(QMouseEvent *event) {
	if (m_panning) {
		const QPoint delta = event->pos() - m_lastPanPos;
		m_lastPanPos = event->pos();
		horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
		verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
		event->accept();
		return;
	}
	QGraphicsView::mouseMoveEvent(event);
}

void BoardView::mouseReleaseEvent(QMouseEvent *event) {
	if (m_panning && keymapOrDefault().matchesMouseButton(QStringLiteral("view.pan"), event->button(),
														   event->modifiers(), InputKind::MouseDrag)) {
		m_panning = false;
		setCursor(m_spaceHeld ? Qt::OpenHandCursor : Qt::ArrowCursor);
		event->accept();
		return;
	}
	QGraphicsView::mouseReleaseEvent(event);
}

void BoardView::keyPressEvent(QKeyEvent *event) {
	const Keymap &keymap = keymapOrDefault();
	if (keymap.matchesKey(QStringLiteral("view.panHold"), event) && !event->isAutoRepeat() && !m_spaceHeld) {
		m_spaceHeld = true;
		m_dragModeBeforeSpace = dragMode();
		setDragMode(QGraphicsView::ScrollHandDrag);
		event->accept();
		return;
	}
	if (keymap.matchesKey(QStringLiteral("view.fit"), event)) {
		fitBoardToWindow();
		event->accept();
		return;
	}
	QGraphicsView::keyPressEvent(event);
}

void BoardView::keyReleaseEvent(QKeyEvent *event) {
	if (keymapOrDefault().matchesKey(QStringLiteral("view.panHold"), event) && !event->isAutoRepeat()) {
		m_spaceHeld = false;
		setDragMode(m_dragModeBeforeSpace);
		event->accept();
		return;
	}
	QGraphicsView::keyReleaseEvent(event);
}

void BoardView::focusInEvent(QFocusEvent *event) {
	QGraphicsView::focusInEvent(event);
	emit focusReceived(this);
}

void BoardView::leaveEvent(QEvent *event) {
	QGraphicsView::leaveEvent(event);
	if (auto *bs = qobject_cast<BoardScene *>(scene())) {
		bs->notifyViewLeave();
	}
}
