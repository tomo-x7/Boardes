#include "viewlink.h"

#include <QScrollBar>

#include "../render/boardview.h"

ViewLinkController::ViewLinkController(QObject *parent) : QObject(parent) {
}

void ViewLinkController::setViews(BoardView *front, BoardView *back) {
	m_front = front;
	m_back = back;
	if (m_front) {
		connect(m_front, &BoardView::zoomChanged, this, &ViewLinkController::onFrontChanged);
		connect(m_front->horizontalScrollBar(), &QScrollBar::valueChanged, this, &ViewLinkController::onFrontChanged);
		connect(m_front->verticalScrollBar(), &QScrollBar::valueChanged, this, &ViewLinkController::onFrontChanged);
	}
	if (m_back) {
		connect(m_back, &BoardView::zoomChanged, this, &ViewLinkController::onBackChanged);
		connect(m_back->horizontalScrollBar(), &QScrollBar::valueChanged, this, &ViewLinkController::onBackChanged);
		connect(m_back->verticalScrollBar(), &QScrollBar::valueChanged, this, &ViewLinkController::onBackChanged);
	}
}

void ViewLinkController::setEnabled(bool on) {
	m_enabled = on;
	if (on && m_front) {
		// 有効化した瞬間に裏面を表面へ合わせる。
		syncTo(m_front, m_back);
	}
}

void ViewLinkController::onFrontChanged() {
	if (!m_enabled || m_syncing) {
		return;
	}
	syncTo(m_front, m_back);
}

void ViewLinkController::onBackChanged() {
	if (!m_enabled || m_syncing) {
		return;
	}
	syncTo(m_back, m_front);
}

void ViewLinkController::syncTo(BoardView *source, BoardView *target) {
	if (!source || !target) {
		return;
	}
	m_syncing = true;
	if (!qFuzzyCompare(source->zoomFactor(), target->zoomFactor())) {
		target->setZoom(source->zoomFactor());
	}
	target->centerOnModel(source->viewCenterModel());
	m_syncing = false;
}
