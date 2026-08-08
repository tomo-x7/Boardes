#pragma once

#include <QDialog>
#include <optional>

#include "inputgesture.h"

class QLabel;
class QDialogButtonBox;
class QPushButton;

// 「次に押した操作を記録します」ダイアログ。1つのキー/マウス入力を捕捉して
// InputGesture を作る (Phase 18)。expectedKind によって待ち受ける入力の種類を
// 固定する (曖昧さを無くすため。1回のクリックだけでは「単発クリック」なのか
// 「ドラッグの開始」なのかを区別できないので、呼び出し側が編集対象のコマンドの
// 種類をあらかじめ指定する)。
class GestureCaptureDialog : public QDialog {
	Q_OBJECT

public:
	explicit GestureCaptureDialog(InputKind expectedKind, QWidget *parent = nullptr);

	// OK で閉じたとき、捕捉できていれば値を返す。
	std::optional<InputGesture> capturedGesture() const {
		return m_captured;
	}

protected:
	void keyPressEvent(QKeyEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseDoubleClickEvent(QMouseEvent *event) override;
	void wheelEvent(QWheelEvent *event) override;
	void showEvent(QShowEvent *event) override;
	void closeEvent(QCloseEvent *event) override;

private:
	InputKind m_expectedKind;
	std::optional<InputGesture> m_captured;
	QLabel *m_promptLabel = nullptr;
	QLabel *m_previewLabel = nullptr;
	QDialogButtonBox *m_buttons = nullptr;
	QPushButton *m_retryButton = nullptr;

	void setCaptured(const InputGesture &g);
	void resetCapture();
	void releaseGrabs();
};
