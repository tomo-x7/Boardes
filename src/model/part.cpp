#include "part.h"

#include <QBuffer>
#include <algorithm>

#include "../core/geometry.h"
#include "../core/units.h"

QString partKindToString(PartKind kind) {
	switch (kind) {
	case PartKind::Normal:
		return QStringLiteral("normal");
	case PartKind::Text:
		return QStringLiteral("text");
	case PartKind::ToolFillTop:
		return QStringLiteral("toolFillTop");
	case PartKind::ToolFillBottom:
		return QStringLiteral("toolFillBottom");
	case PartKind::ToolThruHole:
		return QStringLiteral("toolThruHole");
	case PartKind::DrillHole:
		return QStringLiteral("drillHole");
	}
	return QStringLiteral("normal");
}

PartKind partKindFromString(const QString &s) {
	if (s == QStringLiteral("text")) return PartKind::Text;
	if (s == QStringLiteral("toolFillTop")) return PartKind::ToolFillTop;
	if (s == QStringLiteral("toolFillBottom")) return PartKind::ToolFillBottom;
	if (s == QStringLiteral("toolThruHole")) return PartKind::ToolThruHole;
	if (s == QStringLiteral("drillHole")) return PartKind::DrillHole;
	return PartKind::Normal;
}

void Artwork::encodeFromImage() {
	pngBytes.clear();
	if (image.isNull()) {
		return;
	}
	QBuffer buffer(&pngBytes);
	buffer.open(QIODevice::WriteOnly);
	image.save(&buffer, "PNG");
}

bool Artwork::decodeToImage() {
	if (pngBytes.isEmpty()) {
		return false;
	}
	QImage img;
	if (!img.loadFromData(pngBytes, "PNG")) {
		return false;
	}
	image = img;
	return true;
}

Artwork Artwork::fromChromaKeyed(QImage source, QColor key) {
	Artwork art;
	QImage converted = source.convertToFormat(QImage::Format_ARGB32);
	const QRgb keyRgb = key.rgb() | 0xFF000000u;
	for (int y = 0; y < converted.height(); ++y) {
		QRgb *row = reinterpret_cast<QRgb *>(converted.scanLine(y));
		for (int x = 0; x < converted.width(); ++x) {
			if ((row[x] | 0xFF000000u) == keyRgb) {
				row[x] = 0x00000000u;
			}
		}
	}
	art.image = converted;
	art.chromaKey = key;
	art.encodeFromImage();
	return art;
}

Artwork Artwork::fromImageAsIs(QImage source) {
	Artwork art;
	art.image = source.convertToFormat(QImage::Format_ARGB32);
	art.chromaKey.reset();
	art.encodeFromImage();
	return art;
}

QPoint Part::resolveAnchor() const {
	if (anchorExplicit) {
		return anchor;
	}
	if (!pins.isEmpty()) {
		auto it = std::min_element(pins.begin(), pins.end(),
									[](const Pin &a, const Pin &b) { return a.number < b.number; });
		return it->pos;
	}
	// ピンが無い部品 (テキスト・塗りつぶし等): 画像中心をフル格子刻みに丸める。
	const QSize sz = size();
	return snapPoint(QPoint(sz.width() / 2, sz.height() / 2), units::Pitch);
}

bool Part::matchesQuery(const QString &query) const {
	if (query.isEmpty()) {
		return true;
	}
	if (name.contains(query, Qt::CaseInsensitive) || id.contains(query, Qt::CaseInsensitive)) {
		return true;
	}
	for (const QString &kw : keywords) {
		if (kw.contains(query, Qt::CaseInsensitive)) {
			return true;
		}
	}
	return false;
}
