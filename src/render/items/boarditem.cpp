#include "boarditem.h"

#include <QPainter>

BoardItem::BoardItem(const BoardSpec *board, Side side, QGraphicsItem *parent)
	: QGraphicsItem(parent), m_board(board), m_side(side) {
	rebuild();
}

QRectF BoardItem::boundingRect() const {
	if (!m_board) {
		return QRectF();
	}
	return QRectF(QPointF(0, 0), QSizeF(m_board->size));
}

void BoardItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
	if (m_cache.isNull()) {
		return;
	}
	painter->drawPixmap(0, 0, m_cache);
}

void BoardItem::setBoard(const BoardSpec *board) {
	prepareGeometryChange();
	m_board = board;
	rebuild();
	update();
}

void BoardItem::setSide(Side side) {
	if (m_side == side) {
		return;
	}
	m_side = side;
	rebuild();
	update();
}

void BoardItem::rebuild() {
	if (!m_board || m_board->size.isEmpty()) {
		m_cache = QPixmap();
		return;
	}

	const auto &bg = (m_side == Side::Front) ? m_board->backgroundFront : m_board->backgroundBack;
	const bool hasRaster = bg.has_value() && !bg->image.isNull();

	QImage img(m_board->size, QImage::Format_ARGB32_Premultiplied);
	img.fill(Qt::transparent);
	QPainter p(&img);
	p.setRenderHint(QPainter::Antialiasing, true);

	if (hasRaster) {
		if (m_side == Side::Back) {
			// PasS の裏面 BMP (*_ .bmp) は、ソフト側では一切変換せずそのまま表示する前提で
			// 「裏から見た向き」に人手で描かれている (穴位置・座標ラベルは反転済みだが、
			// ロゴ文字などは可読性のためあえて反転されていないことがある = 単純な
			// 全体ミラーの結果ではない)。BoardScene 側は全アイテムに水平反転をかけるので、
			// ここで先に逆向きに反転しておき、二重反転で元の (=作者が意図した) 見た目に
			// 戻す。取付穴・穴位置などの幾何情報は表面画像から解析した値を使っており、
			// そちらは逆にこの反転で正しく裏面位置になる。
			p.drawImage(0, 0, bg->image.flipped(Qt::Horizontal));
		} else {
			p.drawImage(0, 0, bg->image);
		}
	} else {
		p.fillRect(QRect(QPoint(0, 0), m_board->size), m_board->substrateColor);
	}

	// パラメトリック基板のみ銅箔・ランド・穴をコードで描く (PasS 取込基板は背景画像に
	// 既に全部含まれているので二重に描かない)。
	if (!hasRaster) {
		if (m_board->copper != CopperPattern::None) {
			p.setPen(Qt::NoPen);
			p.setBrush(m_board->copperColor);
			const qreal r = m_board->padDiameter / 2.0;

			if (m_board->copper == CopperPattern::PadPerHole) {
				for (const QPoint &c : m_board->holeCenters()) {
					p.drawEllipse(QPointF(c), r, r);
				}
			} else if (m_board->copper == CopperPattern::StripHorizontal ||
					  m_board->copper == CopperPattern::StripVertical) {
				const bool horizontal = m_board->copper == CopperPattern::StripHorizontal;
				QPen linePen(m_board->copperColor, m_board->padDiameter, Qt::SolidLine, Qt::RoundCap);
				p.setPen(linePen);
				const int primaryCount = horizontal ? m_board->rows : m_board->cols;
				const int secondaryCount = horizontal ? m_board->cols : m_board->rows;
				for (int primary = 0; primary < primaryCount; ++primary) {
					for (int s = 0; s + 1 < secondaryCount; ++s) {
						const int col1 = horizontal ? s : primary;
						const int row1 = horizontal ? primary : s;
						const int col2 = horizontal ? s + 1 : primary;
						const int row2 = horizontal ? primary : s + 1;
						if (!m_board->holeExistsAt(col1, row1) || !m_board->holeExistsAt(col2, row2)) {
							continue;
						}
						const QPoint breakKey(horizontal ? col1 : row1, horizontal ? row1 : col1);
						if (m_board->stripBreaks.contains(breakKey)) {
							continue;
						}
						p.drawLine(m_board->holeCenter(col1, row1), m_board->holeCenter(col2, row2));
					}
				}
				p.setPen(Qt::NoPen);
				for (const QPoint &c : m_board->holeCenters()) {
					p.drawEllipse(QPointF(c), r, r);
				}
			}
		}

		if (m_board->padShape != PadShape::None) {
			p.setPen(Qt::NoPen);
			p.setBrush(m_board->padColor);
			const qreal r = m_board->padDiameter / 2.0;
			for (const QPoint &c : m_board->holeCenters()) {
				if (m_board->padShape == PadShape::Round) {
					p.drawEllipse(QPointF(c), r, r);
				} else {
					p.drawRect(QRectF(c.x() - r, c.y() - r, r * 2, r * 2));
				}
			}
		}

		// 穴そのもの (常に基板色で「くり抜く」ように見せる)。
		p.setPen(Qt::NoPen);
		p.setBrush(m_board->substrateColor);
		const qreal hr = m_board->holeDiameter / 2.0;
		for (const QPoint &c : m_board->holeCenters()) {
			p.drawEllipse(QPointF(c), hr, hr);
		}
		for (const auto &mh : m_board->mountingHoles) {
			p.drawEllipse(QPointF(mh.first), mh.second / 2.0, mh.second / 2.0);
		}
	}

	p.end();
	m_cache = QPixmap::fromImage(img);
}
