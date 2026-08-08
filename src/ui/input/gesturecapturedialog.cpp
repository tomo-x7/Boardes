#include "gesturecapturedialog.h"

#include <QCloseEvent>
#include <QDialogButtonBox>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QWheelEvent>

namespace {
QString promptFor(InputKind kind) {
	switch (kind) {
	case InputKind::Key:
		return QObject::tr("次に押したキーを記録します。(Esc で中止)");
	case InputKind::MouseButton:
		return QObject::tr("次にクリックしたマウスボタンを記録します。(Esc で中止)");
	case InputKind::MouseDouble:
		return QObject::tr("次にダブルクリックしたマウスボタンを記録します。(Esc で中止)");
	case InputKind::MouseDrag:
		return QObject::tr("次に押したマウスボタンを、ドラッグ操作として記録します。(Esc で中止)");
	case InputKind::Wheel:
		return QObject::tr("このダイアログの上でホイールを回してください。(Esc で中止)");
	}
	return QString();
}
bool mouseKind(InputKind kind) {
	return kind == InputKind::MouseButton || kind == InputKind::MouseDouble || kind == InputKind::MouseDrag;
}
}  // namespace

GestureCaptureDialog::GestureCaptureDialog(InputKind expectedKind, QWidget *parent)
	: QDialog(parent), m_expectedKind(expectedKind) {
	setWindowTitle(tr("操作の割り当て"));
	setModal(true);

	auto *layout = new QVBoxLayout(this);
	m_promptLabel = new QLabel(promptFor(m_expectedKind), this);
	m_promptLabel->setWordWrap(true);
	layout->addWidget(m_promptLabel);

	m_previewLabel = new QLabel(this);
	m_previewLabel->setStyleSheet(QStringLiteral("QLabel { font-weight: bold; }"));
	m_previewLabel->setAlignment(Qt::AlignCenter);
	m_previewLabel->setMinimumHeight(32);
	layout->addWidget(m_previewLabel);

	m_buttons = new QDialogButtonBox(this);
	m_retryButton = m_buttons->addButton(tr("やり直し"), QDialogButtonBox::ResetRole);
	m_buttons->addButton(QDialogButtonBox::Ok);
	m_buttons->addButton(QDialogButtonBox::Cancel);
	m_buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
	m_retryButton->setEnabled(false);
	connect(m_retryButton, &QPushButton::clicked, this, &GestureCaptureDialog::resetCapture);
	connect(m_buttons, &QDialogButtonBox::accepted, this, &GestureCaptureDialog::accept);
	connect(m_buttons, &QDialogButtonBox::rejected, this, &GestureCaptureDialog::reject);
	layout->addWidget(m_buttons);

	setFocusPolicy(Qt::StrongFocus);
}

void GestureCaptureDialog::showEvent(QShowEvent *event) {
	QDialog::showEvent(event);
	setFocus();
	grabKeyboard();
	if (mouseKind(m_expectedKind)) {
		grabMouse();
	}
}

void GestureCaptureDialog::closeEvent(QCloseEvent *event) {
	releaseGrabs();
	QDialog::closeEvent(event);
}

void GestureCaptureDialog::releaseGrabs() {
	releaseKeyboard();
	releaseMouse();
}

void GestureCaptureDialog::resetCapture() {
	m_captured.reset();
	m_previewLabel->clear();
	m_buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
	m_retryButton->setEnabled(false);
	grabKeyboard();
	if (mouseKind(m_expectedKind)) {
		grabMouse();
	}
}

void GestureCaptureDialog::setCaptured(const InputGesture &g) {
	m_captured = g;
	m_previewLabel->setText(g.toDisplayString());
	m_buttons->button(QDialogButtonBox::Ok)->setEnabled(true);
	m_retryButton->setEnabled(true);
	releaseGrabs();
}

void GestureCaptureDialog::keyPressEvent(QKeyEvent *event) {
	if (event->key() == Qt::Key_Escape && !m_captured.has_value()) {
		reject();
		return;
	}
	if (m_expectedKind != InputKind::Key || m_captured.has_value()) {
		QDialog::keyPressEvent(event);
		return;
	}
	// 修飾キー単体の押下は無視し、実キーが来るまで待つ。
	switch (event->key()) {
	case Qt::Key_Control:
	case Qt::Key_Shift:
	case Qt::Key_Alt:
	case Qt::Key_Meta:
		return;
	default:
		break;
	}
	InputGesture g;
	g.kind = InputKind::Key;
	g.key = event->key();
	g.nativeScanCode = event->nativeScanCode();
	g.mods = event->modifiers();
	setCaptured(g);
}

void GestureCaptureDialog::mousePressEvent(QMouseEvent *event) {
	if (m_expectedKind != InputKind::MouseButton && m_expectedKind != InputKind::MouseDrag) {
		QDialog::mousePressEvent(event);
		return;
	}
	if (m_captured.has_value()) {
		return;
	}
	InputGesture g;
	g.kind = m_expectedKind;
	g.button = event->button();
	g.mods = event->modifiers();
	setCaptured(g);
}

void GestureCaptureDialog::mouseDoubleClickEvent(QMouseEvent *event) {
	if (m_expectedKind != InputKind::MouseDouble || m_captured.has_value()) {
		QDialog::mouseDoubleClickEvent(event);
		return;
	}
	InputGesture g;
	g.kind = InputKind::MouseDouble;
	g.button = event->button();
	g.mods = event->modifiers();
	setCaptured(g);
}

void GestureCaptureDialog::wheelEvent(QWheelEvent *event) {
	if (m_expectedKind != InputKind::Wheel || m_captured.has_value()) {
		QDialog::wheelEvent(event);
		return;
	}
	InputGesture g;
	g.kind = InputKind::Wheel;
	g.wheelDelta = event->angleDelta().y() >= 0 ? 1 : -1;
	g.mods = event->modifiers();
	setCaptured(g);
}
