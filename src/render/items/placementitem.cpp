#include "placementitem.h"

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
	setFlag(QGraphicsItem::ItemIsSelectable, true);
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

	if (m_showPinNumbers) {
		painter->save();
		// 裏面シーンはシーンのルートで水平反転されているため、ここで局所的に打ち消して
		// 文字が鏡文字にならないようにする (LabelItem と同じ考え方)。
		if (m_viewSide == Side::Back) {
			painter->translate(rect.width(), 0);
			painter->scale(-1, 1);
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
			const qreal lx = (m_viewSide == Side::Back) ? (rect.width() - rp.x()) : rp.x();
			painter->drawText(QPointF(lx + 0.5, rp.y() - 0.5), QString::number(pin.number));
		}
		painter->restore();
	}
}

void PlacementItem::refresh() {
	prepareGeometryChange();
	setPos(m_placement->pos);
	setZValue(m_placement->z);
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
