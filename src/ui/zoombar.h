#pragma once

#include <QWidget>

class QSlider;
class QComboBox;
class QToolButton;
class BoardView;

// ステータスバーに置く倍率バー (Office 等でおなじみの構成)。
//
// 対象ビューは setTargetView() で指定する。MainWindow は表面/裏面それぞれに1個ずつ
// (ステータスバー左右) 常設し、各々固定のビューを操作対象にする (以前は1個をフォーカス中の
// ビューに切り替える方式だったが、両方常時操作できる方が分かりやすいという方針に変更した)。
// スライダーは対数目盛 (見た目のドラッグ量が倍率の相対変化に対応するようにするため)。
class ZoomBar : public QWidget {
	Q_OBJECT

public:
	explicit ZoomBar(QWidget *parent = nullptr);

	// 非所有。nullptr にすると無効表示になる。
	void setTargetView(BoardView *view);

	// 縮小/拡大ボタンのアイコンをテーマ色で作り直す (Theme::changed から呼ばれる想定)。
	void refreshIcons();

private slots:
	void onSliderChanged(int value);
	void onComboActivated(int index);
	void onViewZoomChanged(qreal factor);

private:
	BoardView *m_view = nullptr;
	QToolButton *m_minusButton;
	QToolButton *m_plusButton;
	QSlider *m_slider;
	QComboBox *m_combo;
	bool m_updating = false;

	void applyZoom(qreal factor);
	void syncFromView();
};
