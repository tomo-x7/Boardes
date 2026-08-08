#include "zoombar.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <cmath>

#include "../render/boardview.h"
#include "helphint.h"

namespace {
constexpr int kSliderScale = 100;  // value = round(log2(zoom) * kSliderScale)
constexpr qreal kMinZoom = 0.1;
constexpr qreal kMaxZoom = 32.0;

int zoomToSliderValue(qreal zoom) {
	return qRound(std::log2(zoom) * kSliderScale);
}
qreal sliderValueToZoom(int value) {
	return std::pow(2.0, static_cast<qreal>(value) / kSliderScale);
}
}  // namespace

ZoomBar::ZoomBar(QWidget *parent) : QWidget(parent) {
	auto *minusButton = new QPushButton(QStringLiteral("－"), this);
	minusButton->setFixedWidth(24);
	auto *plusButton = new QPushButton(QStringLiteral("＋"), this);
	plusButton->setFixedWidth(24);

	m_slider = new QSlider(Qt::Horizontal, this);
	m_slider->setFixedWidth(100);
	m_slider->setRange(zoomToSliderValue(kMinZoom), zoomToSliderValue(kMaxZoom));

	m_combo = new QComboBox(this);
	m_combo->setEditable(true);
	m_combo->setFixedWidth(90);
	for (int pct : {25, 50, 75, 100, 150, 200, 400, 800, 1600}) {
		m_combo->addItem(QStringLiteral("%1%").arg(pct), pct);
	}
	m_combo->addItem(QStringLiteral("全体表示"), -1);

	auto *layout = new QHBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(minusButton);
	layout->addWidget(m_slider);
	layout->addWidget(plusButton);
	layout->addWidget(m_combo);
	layout->addWidget(helphint::button(
		tr("表示倍率を変えます。フォーカスのあるビュー (最後にクリックした表面/裏面) が対象です。"), this));

	connect(minusButton, &QPushButton::clicked, this, [this] {
		if (m_view) m_view->zoomOut();
	});
	connect(plusButton, &QPushButton::clicked, this, [this] {
		if (m_view) m_view->zoomIn();
	});
	connect(m_slider, &QSlider::valueChanged, this, &ZoomBar::onSliderChanged);
	connect(m_combo, &QComboBox::activated, this, &ZoomBar::onComboActivated);
	connect(m_combo->lineEdit(), &QLineEdit::editingFinished, this, [this] {
		bool ok = false;
		QString text = m_combo->currentText();
		text.remove(QLatin1Char('%'));
		const int pct = text.trimmed().toInt(&ok);
		if (ok && pct > 0) {
			applyZoom(pct / 100.0);
		}
	});

	setEnabled(false);
}

void ZoomBar::setTargetView(BoardView *view) {
	if (m_view == view) {
		return;
	}
	if (m_view) {
		disconnect(m_view, nullptr, this, nullptr);
	}
	m_view = view;
	if (m_view) {
		connect(m_view, &BoardView::zoomChanged, this, &ZoomBar::onViewZoomChanged);
		connect(m_view, &QObject::destroyed, this, [this] { m_view = nullptr; setEnabled(false); });
	}
	setEnabled(m_view != nullptr);
	syncFromView();
}

void ZoomBar::syncFromView() {
	if (!m_view) {
		return;
	}
	m_updating = true;
	const qreal z = m_view->zoomFactor();
	m_slider->setValue(zoomToSliderValue(z));
	m_combo->setCurrentText(QStringLiteral("%1%").arg(qRound(z * 100)));
	m_updating = false;
}

void ZoomBar::applyZoom(qreal factor) {
	if (!m_view || m_updating) {
		return;
	}
	m_view->setZoom(factor);
}

void ZoomBar::onSliderChanged(int value) {
	if (m_updating) {
		return;
	}
	applyZoom(sliderValueToZoom(value));
}

void ZoomBar::onComboActivated(int index) {
	const int data = m_combo->itemData(index).toInt();
	if (data == -1) {
		if (m_view) {
			m_view->fitBoardToWindow();
		}
		return;
	}
	applyZoom(data / 100.0);
}

void ZoomBar::onViewZoomChanged(qreal) {
	syncFromView();
}
