#pragma once

#include <QGraphicsItem>
#include <QString>

// 部品の No (refDes) / 値 (value) を表示するラベル。
//
// 裏面シーンはルートで水平反転されているため、そのまま子として置くと文字も
// 鏡文字になってしまう。counterMirrored を true にすると、アイテム自身に
// 逆向きの水平反転 (自身の幅を基準にした QTransform) をかけて打ち消し、
// 位置は鏡像のまま・文字だけ正しく読める向きにする。
class LabelItem : public QGraphicsItem {
public:
	explicit LabelItem(QGraphicsItem *parent = nullptr);

	QRectF boundingRect() const override;
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

	void setTexts(const QString &refDes, const QString &value);
	void setAnchor(QPointF unitPos);
	void setCounterMirrored(bool on);

private:
	QString m_refDes;
	QString m_value;
	bool m_counterMirrored = false;
	QRectF m_bounds;

	void recomputeBounds();
	void applyMirrorIfNeeded();
	QString displayText() const;
};
