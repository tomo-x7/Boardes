#include "stats.h"

#include <QFile>
#include <QStringList>
#include <QTextStream>
#include <algorithm>
#include <cmath>

#include "../core/units.h"
#include "document.h"
#include "librarymanager.h"
#include "placement.h"

namespace {

// 2点間の配線区間の長さ (mm)。直交1区間=2.54mm・45度1区間=3.58mm という
// units.h の基準値を、実際の区間の長さ (フル/ハーフピッチ等) に応じて線形にスケールする。
// WireTool が作る区間は常に直交または45度なのでこの2分岐で網羅できるが、万一
// (外部データ等で) それ以外の区間が来た場合はユークリッド距離を45度基準で概算する。
double segmentLengthMm(QPoint a, QPoint b) {
	const int dx = std::abs(b.x() - a.x());
	const int dy = std::abs(b.y() - a.y());
	if (dx == 0 && dy == 0) {
		return 0.0;
	}
	if (dx == 0 || dy == 0) {
		const int runUnits = dx + dy;  // どちらか一方が0
		return (static_cast<double>(runUnits) / units::Pitch) * units::SegmentMm;
	}
	if (dx == dy) {
		return (static_cast<double>(dx) / units::Pitch) * units::DiagonalMm;
	}
	const double runUnits = std::sqrt(static_cast<double>(dx) * dx + static_cast<double>(dy) * dy);
	return (runUnits / units::Pitch) * units::DiagonalMm;
}

QString csvEscape(const QString &field) {
	if (field.contains(QLatin1Char(',')) || field.contains(QLatin1Char('"')) || field.contains(QLatin1Char('\n'))) {
		QString escaped = field;
		escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));
		return QLatin1Char('"') + escaped + QLatin1Char('"');
	}
	return field;
}

}  // namespace

BoardStats StatsEngine::compute(const Document &document, LibraryManager *libraryManager) const {
	BoardStats stats;
	stats.placementCount = document.placements.size();
	stats.wireCount = document.wires.size();

	// --- 配線長 (レイヤ別) ---
	for (const auto &wire : document.wires) {
		double lengthMm = 0.0;
		for (int i = 1; i < wire->points.size(); ++i) {
			lengthMm += segmentLengthMm(wire->points[i - 1], wire->points[i]);
		}
		stats.wireLengthMmByLayer[wire->layer] += lengthMm;
		stats.totalWireLengthMm += lengthMm;
	}

	// --- 穴数・占有面積 ---
	for (const auto &placement : document.placements) {
		const auto part =
			libraryManager ? libraryManager->resolvePart(placement->libraryId, placement->partId) : nullptr;
		if (!part) {
			continue;
		}
		// ToolThruHole 部品のピンは「スルーホール」として別枠で数え、
		// 「部品ピン」との二重計上を避ける。
		if (part->kind == PartKind::ToolThruHole) {
			stats.thruHoleCount += part->pins.size();
		} else {
			stats.componentPinHoleCount += part->pins.size();
		}

		const QSize rotSize = resolvedBoundingSize(part->size(), placement->rot);
		stats.occupiedAreaMm2 += (rotSize.width() * units::MmPerUnit) * (rotSize.height() * units::MmPerUnit);
	}
	for (const auto &wire : document.wires) {
		if (wire->layer != WireLayer::FrontBare || wire->points.isEmpty()) {
			continue;
		}
		stats.frontBareWireEndpointCount += 2;  // 始点・終点 (裏面でジャンパとして半田付けされる想定)
	}
	stats.totalHoleCount = stats.componentPinHoleCount + stats.frontBareWireEndpointCount + stats.thruHoleCount;

	// --- 面積 (基板・占有率) ---
	// 基板未設定のドキュメント (QSize がデフォルトの (-1,-1)) では面積0として扱う
	// (負の寸法同士を掛けて小さな正の値になってしまうのを避ける)。
	const double boardWidthMm = std::max(0, document.board.size.width()) * units::MmPerUnit;
	const double boardHeightMm = std::max(0, document.board.size.height()) * units::MmPerUnit;
	stats.boardAreaMm2 = boardWidthMm * boardHeightMm;
	stats.occupancyRatio = stats.boardAreaMm2 > 0.0 ? stats.occupiedAreaMm2 / stats.boardAreaMm2 : 0.0;

	return stats;
}

QVector<BomRow> StatsEngine::computeBom(const Document &document, LibraryManager *libraryManager) const {
	QVector<BomRow> result;
	QHash<QString, int> indexByKey;         // グループキー -> result 内のインデックス
	QHash<QString, QStringList> refDesByKey;

	for (const auto &placement : document.placements) {
		const QString key =
			placement->libraryId + QChar(0x1f) + placement->partId + QChar(0x1f) + placement->value;
		auto it = indexByKey.constFind(key);
		int idx;
		if (it == indexByKey.constEnd()) {
			const auto part =
				libraryManager ? libraryManager->resolvePart(placement->libraryId, placement->partId) : nullptr;
			const auto lib = libraryManager ? libraryManager->library(placement->libraryId) : nullptr;
			BomRow row;
			row.partName = part ? part->name : placement->partId;
			row.value = placement->value;
			row.libraryName = lib ? lib->name : placement->libraryId;
			idx = result.size();
			result.append(row);
			indexByKey.insert(key, idx);
		} else {
			idx = it.value();
		}
		result[idx].quantity += 1;
		refDesByKey[key].append(placement->refDes.isEmpty() ? QStringLiteral("(refDes未設定)") : placement->refDes);
	}

	for (auto it = indexByKey.constBegin(); it != indexByKey.constEnd(); ++it) {
		result[it.value()].refDesList = refDesByKey.value(it.key()).join(QStringLiteral(", "));
	}
	return result;
}

QString StatsEngine::bomToCsv(const QVector<BomRow> &rows) {
	QStringList lines;
	lines << QStringLiteral("refDes,部品名,値,ライブラリ,数量");
	for (const auto &row : rows) {
		lines << QStringList{csvEscape(row.refDesList), csvEscape(row.partName), csvEscape(row.value),
							  csvEscape(row.libraryName), QString::number(row.quantity)}
					 .join(QLatin1Char(','));
	}
	return lines.join(QStringLiteral("\r\n")) + QStringLiteral("\r\n");
}

bool StatsEngine::saveBomCsv(const QVector<BomRow> &rows, const QString &filePath) {
	QFile file(filePath);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		return false;
	}
	QTextStream stream(&file);
	stream.setEncoding(QStringConverter::Utf8);
	stream.setGenerateByteOrderMark(true);  // Excel 等で文字化けしないよう BOM を付与する
	stream << bomToCsv(rows);
	return true;
}
