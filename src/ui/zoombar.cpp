#include "zoombar.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QSlider>
#include <QToolButton>
#include <cmath>

#include "../render/boardview.h"
#include "icons.h"
#include "theme.h"

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
	m_minusButton = new QToolButton(this);
	m_minusButton->setAutoRaise(true);
	m_minusButton->setFixedSize(20, 20);
	m_minusButton->setToolTip(tr("縮小"));
	m_plusButton = new QToolButton(this);
	m_plusButton->setAutoRaise(true);
	m_plusButton->setFixedSize(20, 20);
	m_plusButton->setToolTip(tr("拡大"));

	m_slider = new QSlider(Qt::Horizontal, this);
	m_slider->setFixedWidth(100);
	m_slider->setRange(zoomToSliderValue(kMinZoom), zoomToSliderValue(kMaxZoom));

	m_combo = new QComboBox(this);
	m_combo->setEditable(true);
	m_combo->setFixedWidth(90);
	m_combo->setObjectName(QStringLiteral("zoomBarCombo"));
	m_combo->setProperty("mono", true);
	for (int pct : {25, 50, 75, 100, 150, 200, 400, 800, 1600}) {
		m_combo->addItem(QStringLiteral("%1%").arg(pct), pct);
	}
	m_combo->addItem(QStringLiteral("全体表示"), -1);

	// 「？」バッジは付けない (縮小/拡大/スライダー/パーセンテージという構成は自明なため)。
	auto *layout = new QHBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(m_minusButton);
	layout->addWidget(m_slider);
	layout->addWidget(m_plusButton);
	layout->addWidget(m_combo);

	refreshIcons();
	connect(&Theme::instance(), &Theme::changed, this, &ZoomBar::refreshIcons);

	connect(m_minusButton, &QToolButton::clicked, this, [this] {
		if (m_view) m_view->zoomOut();
	});
	connect(m_plusButton, &QToolButton::clicked, this, [this] {
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

void ZoomBar::refreshIcons() {
	const QColor normal = Theme::instance().iconNormalColor();
	m_minusButton->setIcon(icons::soloIcon(icons::ZoomBarMinus, normal));
	m_plusButton->setIcon(icons::soloIcon(icons::ZoomBarPlus, normal));
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
