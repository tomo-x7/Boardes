#pragma once

#include <QImage>
#include <QPixmap>
#include <QString>

#include "../core/geometry.h"

// 部品アートワーク (および基板背景画像) の表示用ピクスマップキャッシュ。
//
// 「それなりに大きなラスタでも扱える」ための要。元データは Part::Artwork::image
// (QImage) のまま保持し、表示直前にズーム倍率に応じた mip を作って
// Qt の QPixmapCache (グローバル LRU キャッシュ) に積む。
//   - 縮小表示は SmoothTransformation、拡大表示は FastTransformation
//     (PasS 部品のドット絵感を保つ)。
//   - 90度倍数の回転は非破壊な転置なので、(id, rotation, zoomBucket) の組ごとに
//     1回だけ計算してキャッシュする。
class ArtworkCache {
public:
	static ArtworkCache &instance();

	// artworkId は呼び出し側が用意する安定したキー (例: "lib:R-2" や "board:ICB-504:front")。
	// targetScale はシーン単位から見た「1論理ピクセルが画面上何ピクセルになるか」。
	QPixmap pixmapFor(const QString &artworkId, const QImage &source, Rotation rotation, qreal targetScale);

	// キャッシュ全体をクリアする (テストやライブラリ差し替え時用)。
	void clear();

	// QPixmapCache の総容量 (KB単位)。既定 256MB 相当。
	static void setMemoryLimitKB(int kb);

	// スナップするズームバケット一覧 (昇順)。
	static const QVector<qreal> &zoomBuckets();

private:
	ArtworkCache();
};
