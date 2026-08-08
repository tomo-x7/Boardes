#pragma once

#include <QColor>
#include <QDialog>
#include <QImage>
#include <QWidget>

#include "../model/part.h"

class QLineEdit;
class QComboBox;
class QSpinBox;
class QCheckBox;
class QPushButton;
class QLabel;
class QTableWidget;
class QTableWidgetItem;
class QDialogButtonBox;

// 部品アートワークを表示し、クリックでピンを打っていくためのキャンバス。
// 小さな画像でも打ちやすいよう整数倍率で拡大表示する (最近傍拡大、ドット感を保つ)。
class PartArtworkCanvas : public QWidget {
	Q_OBJECT

public:
	explicit PartArtworkCanvas(QWidget *parent = nullptr);

	void setImage(const QImage &image);
	void setPins(const QVector<Pin> &pins);
	// 基準点 (緑の十字) の表示。anchorPickMode が true のあいだは、次のクリックで
	// クリック位置を基準点にする (ピンの追加/削除より優先する)。
	void setAnchor(QPoint anchor);
	void setAnchorPickMode(bool on);
	QSize sizeHint() const override;

signals:
	// imagePos は画像のピクセル座標系 (部品原点からの相対座標と同じ単位)。
	void clicked(QPoint imagePos, Qt::MouseButton button);

protected:
	void paintEvent(QPaintEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;

private:
	QImage m_image;
	QVector<Pin> m_pins;
	QPoint m_anchor;
	bool m_anchorPickMode = false;
	int m_scale = 1;

	QPoint widgetToImage(const QPoint &widgetPos) const;
	void recomputeScale();
};

// 新規/既存の部品 (Part) を編集するダイアログ。
//
// 使い方: 「画像を開く」でラスタ画像を読み込み → キャンバス左クリックでピンを追加
// (既定は番号付き、連番は削除時も自動的に詰め直される) → 右クリックで直近のピンを削除。
// クロマキーを有効にすると指定色を透過して表示・保存する。
class PartEditorDialog : public QDialog {
	Q_OBJECT

public:
	enum class Mode { Create, Edit };

	explicit PartEditorDialog(QWidget *parent = nullptr);

	// タイトルと OK ボタンの文言を切り替える。setPart() を呼ぶと自動的に Edit になる。
	void setMode(Mode mode);

	// 編集対象を設定する。既存部品のアートワークは (クロマキー処理済みなら) その
	// 透過済み画像をそのまま「元画像」として扱う (元の不透明画像は復元できないため)。
	void setPart(const Part &part);
	Part part() const;

private slots:
	void onOpenImage();
	void onToggleEyedropper(bool on);
	void onToggleAnchorPick(bool on);
	void onResetAnchor();
	void onCanvasClicked(QPoint imagePos, Qt::MouseButton button);
	void onChromaColorButtonClicked();
	void onChromaKeyToggled(bool on);
	void onDeleteSelectedPin();
	void onPinTableItemChanged(QTableWidgetItem *item);
	void onAccept();

private:
	PartArtworkCanvas *m_canvas;
	QLabel *m_hintLabel;
	QLineEdit *m_idEdit;
	QLineEdit *m_nameEdit;
	QLineEdit *m_keywordsEdit;
	QComboBox *m_kindCombo;
	QLineEdit *m_refPrefixEdit;
	QCheckBox *m_chromaKeyCheck;
	QPushButton *m_chromaColorButton;
	QPushButton *m_eyedropperButton;
	QCheckBox *m_drillOnlyModeCheck;
	QSpinBox *m_newPinDrillSpin;
	QTableWidget *m_pinTable;
	QPushButton *m_deletePinButton;
	QLabel *m_anchorLabel;
	QPushButton *m_anchorPickButton;
	QPushButton *m_anchorResetButton;
	QDialogButtonBox *m_buttons = nullptr;

	QImage m_sourceImage;  // ユーザーが読み込んだ画像 (クロマキー適用前の「元画像」)
	QVector<Pin> m_pins;
	QPoint m_anchor;
	bool m_anchorExplicit = false;
	bool m_anchorPickActive = false;
	// 既定値は画像を開いたときに左上画素の色へ差し替える (onOpenImage 参照)。
	// 未読込のあいだはマゼンタ (透過色の慣習的な色) にしておく。
	QColor m_chromaKey{255, 0, 255};
	bool m_eyedropperActive = false;

	void rebuildPinTable();
	void refreshCanvasArtwork();
	void renumberSequentially();
	void updateAnchorLabel();
	QPoint effectiveAnchor() const;
	int nextPinNumber() const;
	static void updateColorButton(QPushButton *button, const QColor &color);
};
