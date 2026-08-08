#include "placementitem.h"

#include <QGraphicsSceneHoverEvent>
#include <QPainter>

#include "../../model/librarymanager.h"
#include "../../model/part.h"
#include "../artworkcache.h"

PlacementItem::PlacementItem(std::shared_ptr<const Placement> placement, LibraryManager *libraryManager,
							 Side viewSide, QGraphicsItem *parent)
	: QGraphicsItem(parent), m_placement(std::move(placement)), m_libraryManager(libraryManager),
	  m_viewSide(viewSide) {
	setPos(m_placement->pos);
	setZValue(m_placement->z);
	setVisible(m_placement->visible);
	setFlag(QGraphicsItem::ItemIsSelectable, true);
	setAcceptHoverEvents(true);
}

QSize PlacementItem::rotatedSizeOrDefault() const {
	if (m_libraryManager) {
		if (const auto part = m_libraryManager->resolvePart(m_placement->libraryId, m_placement->partId)) {
			return resolvedBoundingSize(part->size(), m_placement->rot);
		}
	}
	return QSize(20, 20);  // 部品が見つからない場合のプレースホルダサイズ
}

QRectF PlacementItem::boundingRect() const {
	return QRectF(QPointF(0, 0), QSizeF(rotatedSizeOrDefault()));
}

void PlacementItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
	const auto part =
		m_libraryManager ? m_libraryManager->resolvePart(m_placement->libraryId, m_placement->partId) : nullptr;
	const QRectF rect = boundingRect();

	if (!part) {
		// ライブラリ未インストール等で部品が見つからない場合のプレースホルダ表示。
		painter->setPen(QPen(Qt::red, 0));
		painter->setBrush(QBrush(QColor(255, 0, 0, 50)));
		painter->drawRect(rect);
		painter->drawLine(rect.topLeft(), rect.bottomRight());
		painter->drawLine(rect.topRight(), rect.bottomLeft());
		return;
	}

	const bool isOwnSide = (m_placement->side == m_viewSide);
	const QSize partSize = part->size();

	if (isOwnSide && !m_forceOutline && !part->artwork.isNull()) {
		const QString artId = QStringLiteral("part:%1:%2").arg(m_placement->libraryId, m_placement->partId);
		qreal scale = qAbs(painter->transform().m11());
		if (scale <= 0) {
			scale = 1.0;
		}
		const QPixmap pm = ArtworkCache::instance().pixmapFor(artId, part->artwork.image, m_placement->rot, scale);
		if (!pm.isNull()) {
			painter->drawPixmap(rect, pm, QRectF(pm.rect()));
		}
	} else {
		// 反対面から見た場合、またはアウトライン強制時: 輪郭矩形 + ピンパッドのみ描く
		// (PasS の「部品アウトライン表示」相当。裏面用の絵は別途持たない)。
		painter->setPen(QPen(QColor(90, 90, 90), 0));
		painter->setBrush(Qt::NoBrush);
		painter->drawRect(rect);
		painter->setPen(Qt::NoPen);
		painter->setBrush(QColor(90, 90, 90));
		for (const auto &pin : part->pins) {
			const QPoint rp = rotatePoint(pin.pos, partSize, m_placement->rot);
			painter->drawEllipse(QPointF(rp), 1.2, 1.2);
		}
	}

	// 接点マーカー: どちらの面から見ていても、全ピン位置に丸を重ねる。PasS 部品は
	// 赤いマーカー画素をそのままアートワークに残しているので (Phase 13)、ここで
	// 同じ位置にさらに丸が重なる形になる。独自部品でも同じ表示ができる。
	if (m_pinMarkersVisible) {
		painter->setPen(Qt::NoPen);
		painter->setBrush(m_pinMarkerColor);
		const qreal r = m_pinMarkerDiameter / 2.0;
		for (const auto &pin : part->pins) {
			const QPoint rp = rotatePoint(pin.pos, partSize, m_placement->rot);
			painter->drawEllipse(QPointF(rp), r, r);
		}
	}

	if (m_showPinNumbers) {
		painter->save();
		// 裏面シーンは (BackViewMode に応じて) 水平/垂直反転されているため、ここで
		// 局所的に打ち消して文字が鏡文字にならないようにする (LabelItem と同じ考え方)。
		// 打ち消し変換自体が位置もずらしてしまうので、描画位置 (lx/ly) 側でも
		// 同じ分だけ逆補正して元の (鏡像の) スクリーン位置に戻す。
		if (m_textFlipX) {
			painter->translate(rect.width(), 0);
			painter->scale(-1, 1);
		}
		if (m_textFlipY) {
			painter->translate(0, rect.height());
			painter->scale(1, -1);
		}
		QFont f = painter->font();
		f.setPixelSize(3);
		painter->setFont(f);
		painter->setPen(Qt::black);
		for (const auto &pin : part->pins) {
			if (!pin.hasNumber()) {
				continue;
			}
			const QPoint rp = rotatePoint(pin.pos, partSize, m_placement->rot);
			const qreal lx = m_textFlipX ? (rect.width() - rp.x()) : rp.x();
			const qreal ly = m_textFlipY ? (rect.height() - rp.y()) : rp.y();
			painter->drawText(QPointF(lx + 0.5, ly - 0.5), QString::number(pin.number));
		}
		painter->restore();
	}

	// 選択・ホバーの可視化。画面上で一定の太さになるよう、現在の描画スケールの
	// 逆数を掛けた「デバイス非依存の1px」単位 (px) でペン幅を決める。
	const qreal px = 1.0 / qMax(0.001, qAbs(painter->transform().m11()));
	if (isSelected()) {
		painter->setBrush(QColor(0, 200, 255, 50));
		painter->setPen(QPen(Qt::black, 3 * px));
		painter->drawRect(rect);
		painter->setBrush(Qt::NoBrush);
		painter->setPen(QPen(QColor(0, 220, 255), 1.5 * px, Qt::DashLine));
		painter->drawRect(rect);
		// 四隅のハンドル。
		painter->setPen(Qt::NoPen);
		painter->setBrush(Qt::black);
		const qreal hs = 4 * px;
		for (const QPointF &corner : {rect.topLeft(), rect.topRight(), rect.bottomLeft(), rect.bottomRight()}) {
			painter->drawRect(QRectF(corner.x() - hs / 2, corner.y() - hs / 2, hs, hs));
		}
	} else if (m_hovered) {
		painter->setBrush(Qt::NoBrush);
		painter->setPen(QPen(QColor(0, 200, 255, 160), 1.5 * px));
		painter->drawRect(rect);
	}
}

void PlacementItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event) {
	QGraphicsItem::hoverEnterEvent(event);
	m_hovered = true;
	update();
}

void PlacementItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
	QGraphicsItem::hoverLeaveEvent(event);
	m_hovered = false;
	update();
}

void PlacementItem::refresh() {
	prepareGeometryChange();
	setPos(m_placement->pos);
	setZValue(m_placement->z);
	setVisible(m_placement->visible);
	update();
}

void PlacementItem::setShowPinNumbers(bool show) {
	if (m_showPinNumbers == show) {
		return;
	}
	m_showPinNumbers = show;
	update();
}

void PlacementItem::setForceOutline(bool force) {
	if (m_forceOutline == force) {
		return;
	}
	m_forceOutline = force;
	update();
}

void PlacementItem::setTextFlip(bool flipX, bool flipY) {
	if (m_textFlipX == flipX && m_textFlipY == flipY) {
		return;
	}
	m_textFlipX = flipX;
	m_textFlipY = flipY;
	update();
}

void PlacementItem::setPinMarkers(bool visible, QColor color, qreal diameterUnits) {
	m_pinMarkersVisible = visible;
	m_pinMarkerColor = color;
	m_pinMarkerDiameter = diameterUnits;
	update();
}
