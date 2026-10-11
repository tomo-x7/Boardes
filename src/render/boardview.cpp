#include "boardview.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
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

bool BoardView::startsPan(const QMouseEvent *event) const {
	// Space 押下中の左ボタンは、アクティブなツール (配線の頂点追加など) ではなく
	// ビュー自身のパンに使う。シーンへ渡すと、ツールが押下を消費してパンが始まらない。
	if (m_spaceHeld && event->button() == Qt::LeftButton) {
		return true;
	}
	return keymapOrDefault().matchesMouseButton(QStringLiteral("view.pan"), event->button(), event->modifiers(),
												InputKind::MouseDrag);
}

void BoardView::beginPan(const QPoint &pos, Qt::MouseButton button) {
	m_panning = true;
	m_panButton = button;
	m_lastPanPos = pos;
	setCursor(Qt::ClosedHandCursor);
}

void BoardView::mousePressEvent(QMouseEvent *event) {
	if (startsPan(event)) {
		beginPan(event->pos(), event->button());
		event->accept();
		return;
	}
	QGraphicsView::mousePressEvent(event);
}

void BoardView::mouseDoubleClickEvent(QMouseEvent *event) {
	// 素早い2回目の押下はダブルクリックとして届く。パン用のボタンならパンとして扱い、
	// ツールのダブルクリック処理 (押下扱いで頂点を追加する等) へ渡さない。
	if (startsPan(event)) {
		beginPan(event->pos(), event->button());
		event->accept();
		return;
	}
	QGraphicsView::mouseDoubleClickEvent(event);
}

void BoardView::mouseMoveEvent(QMouseEvent *event) {
	if (auto *bs = qobject_cast<BoardScene *>(scene())) {
		emit cursorModelPosChanged(bs->toModel(mapToScene(event->pos())));
	}
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
	// パンを始めたボタンを離したら終わり。修飾キーまで照合し直すと、パン中に Ctrl 等が
	// 押されただけで終了できなくなり、ボタンを押さない移動でもスクロールし続けてしまう。
	if (m_panning && event->button() == m_panButton) {
		m_panning = false;
		m_panButton = Qt::NoButton;
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

void BoardView::focusOutEvent(QFocusEvent *event) {
	// Space を押したままフォーカスが移ると keyRelease が届かず、Space 押下中の扱いが残ってしまう。
	// 残ったままだと以後の左クリックがすべてパンになりツールが使えなくなるので、ここで解除する。
	if (m_spaceHeld) {
		m_spaceHeld = false;
		setDragMode(m_dragModeBeforeSpace);
	}
	QGraphicsView::focusOutEvent(event);
}

void BoardView::leaveEvent(QEvent *event) {
	QGraphicsView::leaveEvent(event);
	emit cursorLeft();
	if (auto *bs = qobject_cast<BoardScene *>(scene())) {
		bs->notifyViewLeave();
	}
}

void BoardView::drawForeground(QPainter *painter, const QRectF &rect) {
	QGraphicsView::drawForeground(painter, rect);
	auto *bs = qobject_cast<BoardScene *>(scene());
	if (!bs || !bs->boardRect().isEmpty()) {
		return;
	}
	// ビューの座標系 (画面ピクセル) で描く。ズームに関わらず同じ大きさにするため。
	painter->save();
	painter->resetTransform();
	painter->setPen(QColor(170, 170, 170));
	painter->drawText(viewport()->rect(), Qt::AlignCenter, tr("基板が未設定\n「基板」メニューから選択"));
	painter->restore();
}
