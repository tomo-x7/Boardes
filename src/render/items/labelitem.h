#pragma once

#include <QGraphicsItem>
#include <QString>

// 部品の No (refDes) / 値 (value) を表示するラベル。
//
// 裏面シーンはルートで反転されている (BoardScene::BackViewMode に応じて水平/垂直)
// ため、そのまま子として置くと文字も鏡文字になってしまう。setCounterMirror() で
// 逆向きの反転 (アイテム自身の幅/高さを基準にした QTransform) をかけて打ち消し、
// 位置は鏡像のまま・文字だけ正しく読める向きにする。
class LabelItem : public QGraphicsItem {
public:
	explicit LabelItem(QGraphicsItem *parent = nullptr);

	QRectF boundingRect() const override;
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

	void setTexts(const QString &refDes, const QString &value);
	void setAnchor(QPointF unitPos);
	// flipX/flipY: それぞれ親 (BoardScene::m_root) にかかっている水平/垂直反転を、
	// このアイテム自身で打ち消すかどうか。
	void setCounterMirror(bool flipX, bool flipY);

private:
	QString m_refDes;
	QString m_value;
	bool m_flipX = false;
	bool m_flipY = false;
	QRectF m_bounds;

	void recomputeBounds();
	void applyMirrorIfNeeded();
	QString displayText() const;
};
