#pragma once

#include <QColor>
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
	// 裏面ビューの反転モードに応じて、文字 (ピン番号) の向きを補正するための軸。
	// BoardScene::textFlipX()/textFlipY() をそのまま渡す。
	void setTextFlip(bool flipX, bool flipY);
	// 接点 (ピン) の位置マーカー。PasS 部品は赤いマーカー画素をそのまま残しているので、
	// これを ON にすると同じ位置にさらに丸が重なる (独自部品でも表示できる)。
	void setPinMarkers(bool visible, QColor color, qreal diameterUnits);

protected:
	void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
	void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

private:
	std::shared_ptr<const Placement> m_placement;
	LibraryManager *m_libraryManager;
	Side m_viewSide;
	bool m_showPinNumbers = false;
	bool m_forceOutline = false;
	bool m_textFlipX = false;
	bool m_textFlipY = false;
	bool m_hovered = false;
	bool m_pinMarkersVisible = false;
	QColor m_pinMarkerColor{255, 0, 0};
	qreal m_pinMarkerDiameter = 3.0;

	QSize rotatedSizeOrDefault() const;
};
