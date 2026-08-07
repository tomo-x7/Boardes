#pragma once

#include <QGraphicsView>

// 表面/裏面それぞれの基板ビュー。ホイールでカーソル中心ズーム、中ボタンドラッグ/
// スペース押下中の左ドラッグでパン、Ctrl+0 で全体表示。
class BoardView : public QGraphicsView {
	Q_OBJECT

public:
	explicit BoardView(QWidget *parent = nullptr);

	qreal zoomFactor() const {
		return m_zoom;
	}

public slots:
	void zoomIn();
	void zoomOut();
	void resetZoom();
	void fitBoardToWindow();

signals:
	void zoomChanged(qreal factor);

protected:
	void wheelEvent(QWheelEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;
	void keyReleaseEvent(QKeyEvent *event) override;

private:
	qreal m_zoom = 1.0;
	bool m_panning = false;
	bool m_spaceHeld = false;
	QPoint m_lastPanPos;
	QGraphicsView::DragMode m_dragModeBeforeSpace = QGraphicsView::RubberBandDrag;

	void setZoom(qreal factor);
	void beginPan(const QPoint &pos);
};
