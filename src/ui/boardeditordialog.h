#pragma once

#include <QColor>
#include <QDialog>
#include <optional>

#include "../model/board.h"

class QLineEdit;
class QSpinBox;
class QComboBox;
class QCheckBox;
class QLabel;
class QPushButton;

// 新規/既存のパラメトリック基板 (BoardSpec) を編集するダイアログ。
//
// 表裏の背景画像 (任意) を扱える。基板同士の見た目の違いは実質この画像が担うため、
// PasS 取込基板に限らずすべての基板で使える一級機能にしてある。背景画像が無い場合は
// 従来どおり cols/rows/pitch/origin から対称マージンでサイズを自動導出し、Boardes が
// 銅箔・ランド・穴をコードで描く。画像がある場合はその画素サイズが基板サイズになり、
// 描画は画像に一任する (BoardItem::rebuild() の hasRaster 分岐、変更なし)。
// 変形穴配置 (absentHoles)・取付穴・幅マーカー (outlineRect) は取込専用の経路でのみ
// 生成される想定で、このダイアログでは扱わない。
class BoardEditorDialog : public QDialog {
	Q_OBJECT

public:
	explicit BoardEditorDialog(QWidget *parent = nullptr);

	// 編集対象を設定する (呼ばなければ「新規」の既定値のまま)。
	void setBoard(const BoardSpec &board);
	// 現在のフォーム内容から組み立てた BoardSpec を返す (id/name は trim 済み)。
	BoardSpec board() const;

private slots:
	void updateDerivedLabel();
	void pickSubstrateColor();
	void pickPadColor();
	void pickCopperColor();
	void onOpenFrontBackground();
	void onClearFrontBackground();
	void onOpenBackBackground();
	void onClearBackBackground();
	void onAccept();

private:
	QLineEdit *m_idEdit;
	QLineEdit *m_nameEdit;
	QSpinBox *m_colsSpin;
	QSpinBox *m_rowsSpin;
	QSpinBox *m_pitchSpin;
	QSpinBox *m_originXSpin;
	QSpinBox *m_originYSpin;
	QComboBox *m_padShapeCombo;
	QSpinBox *m_padDiameterSpin;
	QSpinBox *m_holeDiameterSpin;
	QComboBox *m_copperCombo;
	QCheckBox *m_doubleSidedCheck;
	QPushButton *m_substrateColorButton;
	QPushButton *m_padColorButton;
	QPushButton *m_copperColorButton;
	QLabel *m_derivedLabel;

	QLabel *m_frontBgThumb;
	QPushButton *m_frontBgClearButton;
	QLabel *m_backBgThumb;
	QPushButton *m_backBgClearButton;

	QColor m_substrateColor{boarddefaults::Substrate};
	QColor m_padColor{boarddefaults::Pad};
	QColor m_copperColor{boarddefaults::Copper};
	std::optional<Artwork> m_backgroundFront;
	std::optional<Artwork> m_backgroundBack;

	static void updateColorButton(QPushButton *button, const QColor &color);
	static void updateThumbnail(QLabel *thumb, const std::optional<Artwork> &art);
	void loadBackgroundInto(std::optional<Artwork> &target, QLabel *thumb, const QString &dialogTitle);
	BoardSpec boardFromFields() const;
};
