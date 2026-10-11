#pragma once

#include <QGraphicsView>

#include "../ui/input/keymap.h"

// 表面/裏面それぞれの基板ビュー。ホイールでカーソル中心ズーム、中ボタンドラッグ/
// スペース押下中の左ドラッグでパン、Ctrl+0 で全体表示 (既定。Phase 18 でカスタマイズ可能)。
class BoardView : public QGraphicsView {
	Q_OBJECT

public:
	explicit BoardView(QWidget *parent = nullptr);

	qreal zoomFactor() const {
		return m_zoom;
	}

	// 非所有。パン/全体表示のキー・マウス割り当てのカスタマイズ (Phase 18)。
	// 未設定 (nullptr) の間は既定値のまま動作する。
	void setKeymap(const Keymap *keymap) {
		m_keymap = keymap;
	}

	// 現在ビューの中心に映っているモデル座標 (BoardScene::toModel 経由)。
	// scene() が BoardScene でなければ (0,0) を返す。表裏ビューの連動 (ViewLinkController)
	// に使う。
	QPointF viewCenterModel() const;
	void centerOnModel(QPointF modelPos);

public slots:
	void zoomIn();
	void zoomOut();
	void setZoom(qreal factor);
	void resetZoom();
	// scene() が BoardScene であればその boardRect() (余白なしの基板外形) に、
	// そうでなければ sceneRect() に合わせる。
	void fitBoardToWindow();

signals:
	void zoomChanged(qreal factor);
	// フォーカスを得た/失った。倍率バー・表裏連動が「今操作しているのはどちらのビューか」を
	// 判定するために使う。
	void focusReceived(BoardView *self);
	// カーソルのモデル座標 (BoardScene::toModel 経由。裏面ビューでも基板の表から見た座標)。
	// ステータスバーの座標表示に使う。ビューの外へ出たら cursorLeft。
	void cursorModelPosChanged(QPointF modelPos);
	void cursorLeft();

protected:
	void wheelEvent(QWheelEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseDoubleClickEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;
	void keyReleaseEvent(QKeyEvent *event) override;
	void focusInEvent(QFocusEvent *event) override;
	void focusOutEvent(QFocusEvent *event) override;
	void leaveEvent(QEvent *event) override;
	// 基板が未設定のあいだ、中央に「基板メニューから選ぶ」案内を出す。
	void drawForeground(QPainter *painter, const QRectF &rect) override;

private:
	qreal m_zoom = 1.0;
	bool m_panning = false;
	// パンを始めたボタン。離したときに修飾キーの状態に関わらずこのボタンでパンを終える。
	Qt::MouseButton m_panButton = Qt::NoButton;
	bool m_spaceHeld = false;
	QPoint m_lastPanPos;
	QGraphicsView::DragMode m_dragModeBeforeSpace = QGraphicsView::RubberBandDrag;
	const Keymap *m_keymap = nullptr;

	const Keymap &keymapOrDefault() const {
		static const Keymap fallback;
		return m_keymap ? *m_keymap : fallback;
	}

	// このマウス押下がビュー自身のパン開始か (ツールには渡さない)。
	bool startsPan(const QMouseEvent *event) const;
	void beginPan(const QPoint &pos, Qt::MouseButton button);
};
