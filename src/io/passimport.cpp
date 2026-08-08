#include "passimport.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <cstdint>

#include "../core/units.h"
#include "shiftjis.h"

// PasS のバイナリ形式の解読結果 (PasS 本体・仕様書には記載が無く、実データ
// 241 部品 + 22 基板を解析して得たもの):
//
//   ピンマーカー画素の判定: R>150 && G<16 && B<16
//     ピン番号   = 255 - R   (0 = 番号なし = 単なるドリル穴)
//     ドリルコード = B*16 + G  (0 = 既定0.9mm、それ以外は code*0.1mm)
//     両方が同時に成立することもある (例: USB-A.bmp の (250,4,1) はピン5かつ2.0mm)
//
//   基板 BMP: 表面 X.bmp と裏面 X_.bmp のペア。穴は純白(255,255,255)の連結成分で、
//     中心は X,Y とも 10 の倍数の格子上にきっちり乗る。取付穴は面積が明らかに大きい。
//     背景色 (基板色) は (187,201,158) が既定。
//
//   カテゴリ名: <Cat>/<Cat>.txt が Shift-JIS。
//   カテゴリアイコン: <Cat>/<Cat>.ico は拡張子を信用できず、実体が BMP のことがある
//     (マジックバイトで判別する)。
namespace passimport {

namespace {

constexpr QRgb kChromaKeyRgb = qRgb(187, 201, 158);
constexpr double kPi = 3.14159265358979323846;

bool isPinMarker(int r, int g, int b) {
	return r > 150 && g < 16 && b < 16;
}

PartKind classifyKind(const QString &categoryId, const QString &baseName) {
	if (categoryId.compare(QStringLiteral("Text"), Qt::CaseInsensitive) == 0) {
		return PartKind::Text;
	}
	if (categoryId.compare(QStringLiteral("Hole"), Qt::CaseInsensitive) == 0) {
		return PartKind::DrillHole;
	}
	if (categoryId.compare(QStringLiteral("Tool"), Qt::CaseInsensitive) == 0) {
		if (baseName.compare(QStringLiteral("Fill Top"), Qt::CaseInsensitive) == 0) {
			return PartKind::ToolFillTop;
		}
		if (baseName.compare(QStringLiteral("Fill Bottom"), Qt::CaseInsensitive) == 0) {
			return PartKind::ToolFillBottom;
		}
		if (baseName.startsWith(QStringLiteral("Thru Hole"), Qt::CaseInsensitive)) {
			return PartKind::ToolThruHole;
		}
	}
	return PartKind::Normal;
}

std::optional<Part> importPartBmp(const QString &filePath, const QString &categoryId, QString *errorOut) {
	QImage loaded;
	if (!loaded.load(filePath)) {
		if (errorOut) *errorOut = QStringLiteral("画像として読み込めませんでした");
		return std::nullopt;
	}
	const QImage src888 = loaded.convertToFormat(QImage::Format_RGB888);
	const int w = src888.width();
	const int h = src888.height();
	if (w <= 0 || h <= 0) {
		if (errorOut) *errorOut = QStringLiteral("画像サイズが不正です");
		return std::nullopt;
	}

	QVector<Pin> pins;

	for (int y = 0; y < h; ++y) {
		const uchar *line = src888.constScanLine(y);
		for (int x = 0; x < w; ++x) {
			const int r = line[x * 3 + 0];
			const int g = line[x * 3 + 1];
			const int b = line[x * 3 + 2];
			if (isPinMarker(r, g, b)) {
				Pin pin;
				pin.number = 255 - r;
				pin.pos = QPoint(x, y);
				pin.drill = b * 16 + g;
				pins.append(pin);
			}
		}
	}

	QImage argb = src888.convertToFormat(QImage::Format_ARGB32);

	// マーカー画素はあえて埋め戻さず、元の色 (赤系) のまま残す (Phase 13)。
	// Boardes 側の「接点マーカー表示」機能 (BoardScene::setPinMarkers) が同じ位置に
	// さらに丸を重ねる形で表示するので、PasS 部品では元のマーカーの上に重なって見える。
	const QRgb chromaOpaque = kChromaKeyRgb | 0xFF000000u;
	for (int y = 0; y < h; ++y) {
		QRgb *line = reinterpret_cast<QRgb *>(argb.scanLine(y));
		for (int x = 0; x < w; ++x) {
			if ((line[x] | 0xFF000000u) == chromaOpaque) {
				line[x] = 0x00000000u;
			}
		}
	}

	Part part;
	part.id = QFileInfo(filePath).completeBaseName();
	part.name = part.id;
	part.refPrefix = categoryId;
	part.kind = classifyKind(categoryId, part.id);
	part.outline = QRect(0, 0, w, h);
	part.pins = pins;
	part.artwork.image = argb;
	// chromaKey は「取り込み後にユーザーが再編集するときの参考情報」用のメタデータであり、
	// PasS 固有の色をそのまま記録する意味は無い (透過処理自体は上ですでに完了している)。
	// PasS 取込資産は「読み込んで使えれば十分」であり「そのまま (PasS の色を保持して) 使える」
	// 必要は無いため、ここでは設定しない。
	part.artwork.encodeFromImage();

	return part;
}

struct Blob {
	QPoint centroid;
	int area = 0;
	int bboxW = 0;
	int bboxH = 0;

	// 実穴は「隙間なく詰まったほぼ正方形/円形」。基板のシルク文字・ロゴは同じ白でも
	// 疎(fillRatio が低い)か、非常に細長い(aspect が低い)ので、それらを弾く判定に使う。
	// 例 (実データ): 実穴 9px(3x3, fillRatio=1.0) / 取付穴 105px(13x13, fillRatio≈0.62) は通過、
	// ロゴ文字 (fillRatio≈0.06) やスロット状の細長い穴 (aspect≈0.1) は弾かれる。
	bool looksHoleLike() const {
		if (bboxW <= 0 || bboxH <= 0) return false;
		const double fillRatio = static_cast<double>(area) / (bboxW * bboxH);
		const double aspect = static_cast<double>(std::min(bboxW, bboxH)) / std::max(bboxW, bboxH);
		return fillRatio >= 0.4 && aspect >= 0.5;
	}
};

QVector<Blob> findWhiteBlobs(const QImage &img888) {
	const int w = img888.width();
	const int h = img888.height();
	QVector<bool> visited(w * h, false);
	QVector<Blob> blobs;
	QVector<QPoint> stack;
	constexpr QRgb kWhite = qRgb(255, 255, 255);

	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x) {
			const int idx = y * w + x;
			if (visited[idx]) continue;
			if (img888.pixel(x, y) != kWhite) {
				visited[idx] = true;
				continue;
			}
			stack.clear();
			stack.append(QPoint(x, y));
			visited[idx] = true;
			qint64 sumX = 0;
			qint64 sumY = 0;
			int area = 0;
			int minX = x, maxX = x, minY = y, maxY = y;
			while (!stack.isEmpty()) {
				const QPoint p = stack.takeLast();
				sumX += p.x();
				sumY += p.y();
				++area;
				minX = std::min(minX, p.x());
				maxX = std::max(maxX, p.x());
				minY = std::min(minY, p.y());
				maxY = std::max(maxY, p.y());
				static const int dx[4] = {1, -1, 0, 0};
				static const int dy[4] = {0, 0, 1, -1};
				for (int k = 0; k < 4; ++k) {
					const int nx = p.x() + dx[k];
					const int ny = p.y() + dy[k];
					if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
					const int nidx = ny * w + nx;
					if (visited[nidx]) continue;
					if (img888.pixel(nx, ny) != kWhite) {
						visited[nidx] = true;
						continue;
					}
					visited[nidx] = true;
					stack.append(QPoint(nx, ny));
				}
			}
			Blob b;
			b.area = area;
			b.centroid = QPoint(static_cast<int>(std::lround(static_cast<double>(sumX) / area)),
								static_cast<int>(std::lround(static_cast<double>(sumY) / area)));
			b.bboxW = maxX - minX + 1;
			b.bboxH = maxY - minY + 1;
			blobs.append(b);
		}
	}
	return blobs;
}

// 値の最頻値を返す (複数候補があれば QHash の反復順で先に見つかったもの)。
int modeOfValues(const QVector<int> &values) {
	QHash<int, int> freq;
	for (int v : values) freq[v]++;
	int best = 0;
	int bestCount = -1;
	for (auto it = freq.constBegin(); it != freq.constEnd(); ++it) {
		if (it.value() > bestCount) {
			bestCount = it.value();
			best = it.key();
		}
	}
	return best;
}

// 昇順ソート後、隣接する値同士の差の最頻値を返す (0以下の差は無視)。検出できなければ 0。
int modeOfPositiveDiffs(QVector<int> values) {
	std::sort(values.begin(), values.end());
	values.erase(std::unique(values.begin(), values.end()), values.end());
	QHash<int, int> diffFreq;
	for (int i = 1; i < values.size(); ++i) {
		const int d = values[i] - values[i - 1];
		if (d > 0) diffFreq[d]++;
	}
	int best = 0;
	int bestCount = -1;
	for (auto it = diffFreq.constBegin(); it != diffFreq.constEnd(); ++it) {
		if (it.value() > bestCount) {
			bestCount = it.value();
			best = it.key();
		}
	}
	return best;
}

std::optional<BoardSpec> importBoardPair(const QString &frontPath, const QString &backPath, QString *errorOut) {
	QImage frontLoaded, backLoaded;
	if (!frontLoaded.load(frontPath)) {
		if (errorOut) *errorOut = QStringLiteral("表面画像を読み込めませんでした");
		return std::nullopt;
	}
	if (!backLoaded.load(backPath)) {
		if (errorOut) *errorOut = QStringLiteral("裏面画像を読み込めませんでした");
		return std::nullopt;
	}
	const QImage front888 = frontLoaded.convertToFormat(QImage::Format_RGB888);
	const QImage back888 = backLoaded.convertToFormat(QImage::Format_RGB888);
	const int w = front888.width();
	const int h = front888.height();

	const QVector<Blob> blobs = findWhiteBlobs(front888);
	if (blobs.isEmpty()) {
		if (errorOut) *errorOut = QStringLiteral("穴 (白色ブロブ) が見つかりませんでした");
		return std::nullopt;
	}

	// 実際の基板 BMP には、穴以外にも白色のシルク文字やロゴ、細長いスロット状の
	// 切り欠きなどが同じ純白で描かれていることがある (例: ICB-504 の "Sunhayato" ロゴ、
	// FREE 系ボードの目盛りドット模様)。まず looksHoleLike() で「隙間なく詰まった
	// 正方形/円形に近い」ブロブだけに絞り込んでから、その中で最も出現頻度の高い面積
	// (= 実穴のサイズ、同一基板内では常に一定) を探す。この2段構えでロゴ・文字・
	// スロットを確実に除外できる (面積のみによる閾値判定は、これらに引きずられて
	// ピッチ検出が壊れることがあるため使わない)。
	QVector<Blob> candidates;
	for (const auto &b : blobs) {
		if (b.area >= 5 && b.looksHoleLike()) {
			candidates.append(b);
		}
	}
	if (candidates.isEmpty()) {
		candidates = blobs;
	}

	QVector<int> candidateAreas;
	candidateAreas.reserve(candidates.size());
	for (const auto &b : candidates) candidateAreas.append(b.area);
	const int modeArea = modeOfValues(candidateAreas);

	QVector<Blob> gridBlobs, mountingBlobs;
	for (const auto &b : candidates) {
		if (b.area == modeArea) {
			gridBlobs.append(b);
		} else if (b.area > modeArea * 3) {
			// 実穴サイズの3倍を超えるものだけを取付穴とみなす。実穴と取付穴は
			// 実データで10倍以上の面積差があるため、この間 (中途半端なサイズ) の
			// ブロブは分類不能なノイズとして捨てる。
			mountingBlobs.append(b);
		}
	}
	if (gridBlobs.isEmpty()) {
		gridBlobs = candidates;
		mountingBlobs.clear();
	}

	QVector<int> xs, ys;
	xs.reserve(gridBlobs.size());
	ys.reserve(gridBlobs.size());
	for (const auto &b : gridBlobs) {
		xs.append(b.centroid.x());
		ys.append(b.centroid.y());
	}
	const int pitchX = modeOfPositiveDiffs(xs);
	const int pitchY = modeOfPositiveDiffs(ys);
	int pitch = pitchX > 0 ? pitchX : (pitchY > 0 ? pitchY : units::Pitch);
	if (pitch <= 0) pitch = units::Pitch;

	const int minX = *std::min_element(xs.begin(), xs.end());
	const int minY = *std::min_element(ys.begin(), ys.end());
	const int maxX = *std::max_element(xs.begin(), xs.end());
	const int maxY = *std::max_element(ys.begin(), ys.end());

	BoardSpec board;
	board.size = QSize(w, h);
	board.origin = QPoint(minX, minY);
	board.pitch = pitch;
	board.cols = (maxX - minX) / pitch + 1;
	board.rows = (maxY - minY) / pitch + 1;

	const int tolerance = std::max(2, pitch / 3);
	for (int row = 0; row < board.rows; ++row) {
		for (int col = 0; col < board.cols; ++col) {
			const QPoint expected = board.holeCenter(col, row);
			bool found = false;
			for (const auto &b : gridBlobs) {
				if (std::abs(b.centroid.x() - expected.x()) <= tolerance &&
					std::abs(b.centroid.y() - expected.y()) <= tolerance) {
					found = true;
					break;
				}
			}
			if (!found) {
				board.absentHoles.append(QPoint(col, row));
			}
		}
	}

	for (const auto &mb : mountingBlobs) {
		const double diameter = 2.0 * std::sqrt(static_cast<double>(mb.area) / kPi);
		board.mountingHoles.append({mb.centroid, static_cast<int>(std::lround(diameter))});
	}

	board.padShape = PadShape::None;
	board.copper = CopperPattern::None;
	// substrateColor は Boardes の既定色 (boarddefaults::Substrate) のままにする。背景画像が
	// 見た目のすべてを担うので描画には影響しない。以前はここで BMP の最頻色 (=PasS の
	// クロマキー色そのもの) を焼き込んでいたが、PasS 資産は「読み込んで使えれば十分」であり
	// 「PasS の色をそのまま保持する」必要は無いため、保存データに残さないようにした。

	// 幅マーカー: y=0 に黒(0,0,0)がちょうど2個あれば、その x 範囲を外形とする。
	QVector<int> blackXs;
	for (int x = 0; x < w; ++x) {
		if (front888.pixel(x, 0) == qRgb(0, 0, 0)) {
			blackXs.append(x);
		}
	}
	if (blackXs.size() == 2) {
		board.outlineRect = QRect(blackXs[0], 0, blackXs[1] - blackXs[0] + 1, h);
	}

	board.backgroundFront = Artwork::fromImageAsIs(front888);
	board.backgroundBack = Artwork::fromImageAsIs(back888);

	return board;
}

QImage decodeIco(const QByteArray &bytes) {
	if (bytes.size() < 6) return {};
	auto u16 = [&](int off) -> uint16_t {
		return static_cast<uint16_t>(static_cast<uint8_t>(bytes[off]) |
									 (static_cast<uint16_t>(static_cast<uint8_t>(bytes[off + 1])) << 8));
	};
	auto u32 = [&](int off) -> uint32_t {
		return static_cast<uint32_t>(static_cast<uint8_t>(bytes[off])) |
			   (static_cast<uint32_t>(static_cast<uint8_t>(bytes[off + 1])) << 8) |
			   (static_cast<uint32_t>(static_cast<uint8_t>(bytes[off + 2])) << 16) |
			   (static_cast<uint32_t>(static_cast<uint8_t>(bytes[off + 3])) << 24);
	};

	if (u16(0) != 0 || u16(2) != 1) return {};  // ICONDIR: reserved=0, type=1(icon)
	const int count = u16(4);
	if (count <= 0 || bytes.size() < 6 + 16) return {};

	const uint32_t imgSize = u32(6 + 8);
	const uint32_t imgOffset = u32(6 + 12);
	if (imgSize < 8 || imgOffset + imgSize > static_cast<uint32_t>(bytes.size())) return {};
	const int base = static_cast<int>(imgOffset);

	// PNG 埋め込み形式 (Vista 以降の大きいアイコンで使われる)。
	if (static_cast<uint8_t>(bytes[base]) == 0x89 && bytes[base + 1] == 'P') {
		QImage img;
		img.loadFromData(bytes.mid(base, static_cast<int>(imgSize)), "PNG");
		return img;
	}

	if (imgSize < 40) return {};
	const uint32_t headerSize = u32(base + 0);
	if (headerSize < 40) return {};
	const int32_t rawWidth = static_cast<int32_t>(u32(base + 4));
	const int32_t rawHeight = static_cast<int32_t>(u32(base + 8));  // XOR+AND 合計 (実高さはこの半分)
	const uint16_t bpp = u16(base + 14);
	const uint32_t compression = u32(base + 16);
	if (compression != 0) return {};

	const int width = rawWidth;
	const int height = rawHeight / 2;
	if (width <= 0 || height <= 0 || width > 256 || height > 256) return {};

	const int pixelDataOffset = base + static_cast<int>(headerSize);

	// AND マスク (1bpp、透明画素を示す) を読むための共通ヘルパー。
	// 実物の PasS カテゴリアイコン (2003年頃の単色線画アイコン) は、色データ側が
	// ほぼ単色 (パレット index 0 = 黒) しか使っておらず、形そのものは AND マスクだけで
	// 表現されている。マスクを無視すると「全面黒の正方形」になってしまうため、
	// どのビット深度でも必ず適用する。
	auto applyAndMask = [&](QImage &img, int xorDataOffset, int xorDataSize) {
		const int maskStride = ((width * 1 + 31) / 32) * 4;
		const int maskOffset = xorDataOffset + xorDataSize;
		if (maskOffset + maskStride * height > bytes.size()) {
			return;  // マスクが無い/壊れている場合は色データそのまま (不透明) にする
		}
		for (int y = 0; y < height; ++y) {
			const int rowStart = maskOffset + (height - 1 - y) * maskStride;
			for (int x = 0; x < width; ++x) {
				const uint8_t byte = static_cast<uint8_t>(bytes[rowStart + x / 8]);
				const bool transparent = ((byte >> (7 - (x % 8))) & 0x01) != 0;
				if (transparent) {
					QColor c = img.pixelColor(x, y);
					c.setAlpha(0);
					img.setPixelColor(x, y, c);
				}
			}
		}
	};

	if (bpp == 24) {
		const int stride = ((width * 3 + 3) / 4) * 4;
		const int xorSize = stride * height;
		if (pixelDataOffset + xorSize > bytes.size()) return {};
		QImage img(width, height, QImage::Format_ARGB32);
		for (int y = 0; y < height; ++y) {
			const int srcRow = pixelDataOffset + (height - 1 - y) * stride;
			for (int x = 0; x < width; ++x) {
				const int s = srcRow + x * 3;
				img.setPixelColor(x, y, QColor(static_cast<uchar>(bytes[s + 2]), static_cast<uchar>(bytes[s + 1]),
											   static_cast<uchar>(bytes[s + 0])));
			}
		}
		applyAndMask(img, pixelDataOffset, xorSize);
		return img;
	}
	if (bpp == 32) {
		const int stride = width * 4;
		const int xorSize = stride * height;
		if (pixelDataOffset + xorSize > bytes.size()) return {};
		QImage img(width, height, QImage::Format_ARGB32);
		for (int y = 0; y < height; ++y) {
			const int srcRow = pixelDataOffset + (height - 1 - y) * stride;
			for (int x = 0; x < width; ++x) {
				const int s = srcRow + x * 4;
				img.setPixelColor(x, y,
								  QColor(static_cast<uchar>(bytes[s + 2]), static_cast<uchar>(bytes[s + 1]),
										 static_cast<uchar>(bytes[s + 0]), static_cast<uchar>(bytes[s + 3])));
			}
		}
		// 32bpp は本来アルファチャンネル自身で透明度を持つが、レガシーなツールが
		// アルファを全て不透明のまま AND マスクだけで表現している場合もあるため、
		// マスク側も併せて適用する (マスクが「全て不透明」なら実質何もしない)。
		applyAndMask(img, pixelDataOffset, xorSize);
		return img;
	}
	if (bpp == 8 || bpp == 4 || bpp == 1) {
		// パレット (BGR + 予約バイトの4バイト単位) が BITMAPINFOHEADER の直後に続く。
		// biClrUsed==0 は「2^bpp 色すべて使用」を意味する慣習。
		const uint32_t clrUsed = u32(base + 32);
		const int paletteEntries = clrUsed != 0 ? static_cast<int>(clrUsed) : (1 << bpp);
		const int paletteBytes = paletteEntries * 4;
		const int paletteOffset = pixelDataOffset;
		if (paletteOffset + paletteBytes > bytes.size()) return {};

		QVector<QRgb> palette(paletteEntries);
		for (int i = 0; i < paletteEntries; ++i) {
			const int o = paletteOffset + i * 4;
			palette[i] = qRgb(static_cast<uint8_t>(bytes[o + 2]), static_cast<uint8_t>(bytes[o + 1]),
							  static_cast<uint8_t>(bytes[o + 0]));
		}
		const int pixelsOffset = paletteOffset + paletteBytes;
		const int stride = ((width * bpp + 31) / 32) * 4;  // DIB の行は4byte境界にパディングされる
		const int xorSize = stride * height;
		if (pixelsOffset + xorSize > bytes.size()) return {};

		auto paletteIndexAt = [&](int rowStart, int x) -> int {
			if (bpp == 8) {
				return static_cast<uint8_t>(bytes[rowStart + x]);
			}
			if (bpp == 4) {
				const uint8_t byte = static_cast<uint8_t>(bytes[rowStart + x / 2]);
				return (x % 2 == 0) ? (byte >> 4) : (byte & 0x0F);
			}
			// bpp == 1
			const uint8_t byte = static_cast<uint8_t>(bytes[rowStart + x / 8]);
			return (byte >> (7 - (x % 8))) & 0x01;
		};

		QImage img(width, height, QImage::Format_ARGB32);
		for (int y = 0; y < height; ++y) {
			const int rowStart = pixelsOffset + (height - 1 - y) * stride;
			for (int x = 0; x < width; ++x) {
				const int idx = paletteIndexAt(rowStart, x);
				const QRgb c = (idx >= 0 && idx < palette.size()) ? palette[idx] : qRgb(0, 0, 0);
				img.setPixelColor(x, y, QColor(c));
			}
		}
		applyAndMask(img, pixelsOffset, xorSize);
		return img;
	}
	return {};
}

// <Cat>.ico は拡張子を信用できない (実体が BMP のことがある) のでマジックバイトで判別する。
QImage sniffIcon(const QByteArray &bytes) {
	if (bytes.size() >= 4 && static_cast<uint8_t>(bytes[0]) == 0x00 && static_cast<uint8_t>(bytes[1]) == 0x00 &&
		static_cast<uint8_t>(bytes[2]) == 0x01 && static_cast<uint8_t>(bytes[3]) == 0x00) {
		return decodeIco(bytes);
	}
	if (bytes.size() >= 2 && bytes[0] == 'B' && bytes[1] == 'M') {
		QImage img;
		img.loadFromData(bytes, "BMP");
		return img;
	}
	QImage img;
	img.loadFromData(bytes);
	return img;
}

}  // namespace

ImportResult importFromDirectory(const QString &sourceDir, Library &lib) {
	ImportResult result;
	QDir root(sourceDir);
	if (!root.exists()) {
		result.error = QStringLiteral("フォルダが見つかりません: %1").arg(sourceDir);
		return result;
	}

	const QFileInfoList categoryDirs = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
	if (categoryDirs.isEmpty()) {
		result.error = QStringLiteral("カテゴリフォルダが見つかりません (PasS の parts フォルダを指定してください)");
		return result;
	}

	for (const QFileInfo &catDirInfo : categoryDirs) {
		const QString categoryId = catDirInfo.fileName();
		QDir catDir(catDirInfo.absoluteFilePath());

		if (categoryId.compare(QStringLiteral("Board"), Qt::CaseInsensitive) == 0) {
			const QFileInfoList bmpFiles = catDir.entryInfoList(QStringList{"*.bmp"}, QDir::Files, QDir::Name);
			QSet<QString> handled;
			for (const QFileInfo &fi : bmpFiles) {
				const QString baseName = fi.completeBaseName();
				if (baseName.endsWith(QLatin1Char('_')) || handled.contains(baseName)) {
					continue;
				}
				handled.insert(baseName);

				const QString backPath = catDir.filePath(baseName + QStringLiteral("_.bmp"));
				if (!QFileInfo::exists(backPath)) {
					result.issues.append(
						{fi.fileName(), QStringLiteral("裏面ファイル (%1_.bmp) が見つかりません").arg(baseName)});
					continue;
				}
				QString err;
				auto board = importBoardPair(fi.absoluteFilePath(), backPath, &err);
				if (!board) {
					result.issues.append({fi.fileName(), err});
					continue;
				}
				board->id = baseName;
				board->name = baseName;
				if (lib.boards.contains(board->id)) {
					result.issues.append({fi.fileName(), QStringLiteral("基板 id が重複したためスキップしました")});
					continue;
				}
				lib.boards.insert(board->id, std::make_shared<BoardSpec>(*board));
				result.boardCount++;
			}
			continue;
		}

		QString categoryName = categoryId;
		const QString txtPath = catDir.filePath(categoryId + QStringLiteral(".txt"));
		if (QFileInfo::exists(txtPath)) {
			QFile txtFile(txtPath);
			if (txtFile.open(QIODevice::ReadOnly)) {
				const QString decoded = shiftjis::decode(txtFile.readAll()).trimmed();
				if (!decoded.isEmpty()) {
					categoryName = decoded;
				}
			}
		}

		CategoryInfo catInfo;
		catInfo.id = categoryId;
		catInfo.name = categoryName;
		catInfo.order = lib.categories.size();

		const QString icoPath = catDir.filePath(categoryId + QStringLiteral(".ico"));
		if (QFileInfo::exists(icoPath)) {
			QFile icoFile(icoPath);
			if (icoFile.open(QIODevice::ReadOnly)) {
				catInfo.icon = sniffIcon(icoFile.readAll());
			}
		}
		lib.categories.append(catInfo);

		const QFileInfoList bmpFiles = catDir.entryInfoList(QStringList{"*.bmp"}, QDir::Files, QDir::Name);
		for (const QFileInfo &fi : bmpFiles) {
			QString err;
			auto part = importPartBmp(fi.absoluteFilePath(), categoryId, &err);
			if (!part) {
				result.issues.append({fi.fileName(), err});
				continue;
			}
			if (lib.parts.contains(part->id)) {
				result.issues.append(
					{fi.fileName(), QStringLiteral("部品 id (%1) が既存の部品と重複したためスキップしました")
										.arg(part->id)});
				continue;
			}
			lib.parts.insert(part->id, std::make_shared<Part>(*part));
			lib.partCategory.insert(part->id, categoryId);
			result.partCount++;
		}
	}

	result.categoryCount = lib.categories.size();
	result.ok = true;
	return result;
}

}  // namespace passimport
