#include "artworkcache.h"

#include <QPixmapCache>
#include <QTransform>

namespace {

QImage rotateImage(const QImage &src, Rotation rot) {
	if (rot == Rotation::R0 || src.isNull()) {
		return src;
	}
	QTransform t;
	t.rotate(static_cast<int>(rot));
	return src.transformed(t);
}

qreal nearestBucketAtLeast(qreal scale) {
	const auto &buckets = ArtworkCache::zoomBuckets();
	for (qreal b : buckets) {
		if (b >= scale) {
			return b;
		}
	}
	return buckets.last();
}

}  // namespace

ArtworkCache::ArtworkCache() {
	setMemoryLimitKB(256 * 1024);
}

ArtworkCache &ArtworkCache::instance() {
	static ArtworkCache cache;
	return cache;
}

void ArtworkCache::setMemoryLimitKB(int kb) {
	QPixmapCache::setCacheLimit(kb);
}

const QVector<qreal> &ArtworkCache::zoomBuckets() {
	static const QVector<qreal> buckets = {0.25, 0.5, 1.0, 2.0, 4.0, 8.0};
	return buckets;
}

void ArtworkCache::clear() {
	QPixmapCache::clear();
}

QPixmap ArtworkCache::pixmapFor(const QString &artworkId, const QImage &source, Rotation rotation, qreal targetScale) {
	if (source.isNull()) {
		return {};
	}
	const qreal bucket = nearestBucketAtLeast(targetScale);
	const QString key = QStringLiteral("boardes-art:%1:%2:%3").arg(artworkId).arg(static_cast<int>(rotation)).arg(bucket);

	QPixmap pixmap;
	if (QPixmapCache::find(key, &pixmap)) {
		return pixmap;
	}

	const QImage rotated = rotateImage(source, rotation);
	QImage scaled;
	if (qFuzzyCompare(bucket, 1.0)) {
		scaled = rotated;
	} else {
		const QSize targetSize(qMax(1, qRound(rotated.width() * bucket)), qMax(1, qRound(rotated.height() * bucket)));
		scaled = rotated.scaled(targetSize, Qt::KeepAspectRatio,
								bucket < 1.0 ? Qt::SmoothTransformation : Qt::FastTransformation);
	}
	pixmap = QPixmap::fromImage(scaled);
	QPixmapCache::insert(key, pixmap);
	return pixmap;
}
