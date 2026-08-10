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
class QDialogButtonBox;
class QRadioButton;
class QButtonGroup;
class QFormLayout;
class QHBoxLayout;

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
	enum class Mode { Create, Edit };

	explicit BoardEditorDialog(QWidget *parent = nullptr);

	// タイトルと OK ボタンの文言を切り替える。setBoard() を呼ぶと自動的に Edit になる。
	void setMode(Mode mode);

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
	void onCreationModeToggled();
	void onAccept();

private:
	// 基板作成モード。画像モードのときは背景画像がすべての見た目を担うので、
	// 色・パッド形状・銅箔パターンの指定は無意味 (かつ紛らわしい) として隠す。
	QRadioButton *m_imageModeRadio;
	QRadioButton *m_paramModeRadio;
	QButtonGroup *m_modeGroup;
	QFormLayout *m_form = nullptr;

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
	// 正方形スウォッチ (色そのもの) + 隣に等幅フォントの hex 表示。仕様書§11。
	QPushButton *m_substrateColorButton;
	QLabel *m_substrateColorHexLabel;
	QPushButton *m_padColorButton;
	QLabel *m_padColorHexLabel;
	QPushButton *m_copperColorButton;
	QLabel *m_copperColorHexLabel;
	QLabel *m_derivedLabel;
	QDialogButtonBox *m_buttons = nullptr;

	QLabel *m_frontBgThumb;
	QPushButton *m_frontBgClearButton;
	QLabel *m_backBgThumb;
	QPushButton *m_backBgClearButton;

	// setRowVisible() で出し入れする行 (画像モード/パラメトリックモードの切替用)。
	QHBoxLayout *m_colorRow = nullptr;
	QHBoxLayout *m_frontBgRow = nullptr;
	QHBoxLayout *m_backBgRow = nullptr;

	QColor m_substrateColor{boarddefaults::Substrate};
	QColor m_padColor{boarddefaults::Pad};
	QColor m_copperColor{boarddefaults::Copper};
	std::optional<Artwork> m_backgroundFront;
	std::optional<Artwork> m_backgroundBack;

	static void updateColorButton(QPushButton *swatch, QLabel *hexLabel, const QColor &color);
	static void updateThumbnail(QLabel *thumb, const std::optional<Artwork> &art);
	void loadBackgroundInto(std::optional<Artwork> &target, QLabel *thumb, const QString &dialogTitle);
	BoardSpec boardFromFields() const;
};
