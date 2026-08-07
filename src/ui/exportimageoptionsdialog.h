#pragma once

#include <QDialog>

#include "../io/imageexport.h"

class QRadioButton;
class QComboBox;
class QCheckBox;

// PNG/SVG/クリップボードのいずれでも共通で使う「対象・拡大率・背景透過」設定ダイアログ。
class ExportImageOptionsDialog : public QDialog {
	Q_OBJECT

public:
	explicit ExportImageOptionsDialog(QWidget *parent = nullptr);

	imageexport::Options options() const;

private:
	QRadioButton *m_frontRadio;
	QRadioButton *m_backRadio;
	QRadioButton *m_bothRadio;
	QComboBox *m_scaleCombo;
	QCheckBox *m_transparentCheck;
};
