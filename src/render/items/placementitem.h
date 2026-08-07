#pragma once

#include <QGraphicsItem>
#include <memory>

#include "../../core/geometry.h"
#include "../../model/placement.h"

class LibraryManager;

// 基板上に配置された1部品の描画アイテム。
//
// m_placement は Document::placements の要素 (shared_ptr) をそのまま指す。
// 表裏どちらのシーンにも同じ Placement データから1つずつ PlacementItem を作る:
// 自分の面 (m_viewSide) と placement->side が一致するときだけフルカラーの
// アートワークを描き、逆の面ではアウトライン+ピンパッドだけを描く
// (PasS の「部品アウトライン表示」相当。裏面から見た部品の見た目データは持たない)。
class PlacementItem : public QGraphicsItem {
public:
	PlacementItem(std::shared_ptr<const Placement> placement, LibraryManager *libraryManager, Side viewSide,
				 QGraphicsItem *parent = nullptr);

	QRectF boundingRect() const override;
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

	std::shared_ptr<const Placement> placement() const {
		return m_placement;
	}

	// placement の内容 (位置・回転など) が変わった後に呼ぶ。
	void refresh();

	void setShowPinNumbers(bool show);
	// PasS の「部品アウトライン表示」相当。true の間は自面の部品もアウトラインのみで描く。
	void setForceOutline(bool force);

private:
	std::shared_ptr<const Placement> m_placement;
	LibraryManager *m_libraryManager;
	Side m_viewSide;
	bool m_showPinNumbers = false;
	bool m_forceOutline = false;

	QSize rotatedSizeOrDefault() const;
};
