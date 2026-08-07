#include "board.h"

#include <cmath>

QString padShapeToString(PadShape s) {
	switch (s) {
	case PadShape::None:
		return QStringLiteral("none");
	case PadShape::Round:
		return QStringLiteral("round");
	case PadShape::Square:
		return QStringLiteral("square");
	}
	return QStringLiteral("round");
}

PadShape padShapeFromString(const QString &s) {
	if (s == QStringLiteral("none")) return PadShape::None;
	if (s == QStringLiteral("square")) return PadShape::Square;
	return PadShape::Round;
}

QString copperPatternToString(CopperPattern c) {
	switch (c) {
	case CopperPattern::None:
		return QStringLiteral("none");
	case CopperPattern::PadPerHole:
		return QStringLiteral("padPerHole");
	case CopperPattern::StripHorizontal:
		return QStringLiteral("stripHorizontal");
	case CopperPattern::StripVertical:
		return QStringLiteral("stripVertical");
	}
	return QStringLiteral("padPerHole");
}

CopperPattern copperPatternFromString(const QString &s) {
	if (s == QStringLiteral("none")) return CopperPattern::None;
	if (s == QStringLiteral("stripHorizontal")) return CopperPattern::StripHorizontal;
	if (s == QStringLiteral("stripVertical")) return CopperPattern::StripVertical;
	return CopperPattern::PadPerHole;
}

bool BoardSpec::holeExistsAt(int col, int row) const {
	if (col < 0 || row < 0 || col >= cols || row >= rows) {
		return false;
	}
	const QPoint p(col, row);
	return !absentHoles.contains(p);
}

QVector<QPoint> BoardSpec::holeCenters() const {
	QVector<QPoint> out;
	out.reserve(cols * rows);
	for (int r = 0; r < rows; ++r) {
		for (int c = 0; c < cols; ++c) {
			if (holeExistsAt(c, r)) {
				out.append(holeCenter(c, r));
			}
		}
	}
	return out;
}

QPoint BoardSpec::nearestGridIndex(QPoint unitPos) const {
	const QPoint rel = unitPos - origin;
	const int col = static_cast<int>(std::lround(static_cast<double>(rel.x()) / pitch));
	const int row = static_cast<int>(std::lround(static_cast<double>(rel.y()) / pitch));
	return QPoint(col, row);
}
