#pragma once

#include <QHash>
#include <QString>
#include <QVector>

#include "wire.h"

class Document;
class LibraryManager;

// 基板全体の集計統計。PasS の統計ダイアログに相当する概算値であり、
// 「同じ穴を複数の要素が共有している」等の重複は特に除去しない
// (PasS 自身の統計も同様の単純合算であるため、桁数が合っていれば十分とする)。
struct BoardStats {
	// 配線長 (レイヤ別、mm)。直交1区間 = 2.54mm、45度1区間 = 3.58mm を基準に、
	// 実際の区間長 (ハーフピッチ等) に応じて線形にスケールする。
	QHash<WireLayer, double> wireLengthMmByLayer;
	double totalWireLengthMm = 0.0;

	// 穴数 (概算)。「部品ピン」は ToolThruHole 部品のぶんを含まない
	// (スルーホールとして別枠で数えるため、二重計上を避ける)。
	int componentPinHoleCount = 0;
	int frontBareWireEndpointCount = 0;
	int thruHoleCount = 0;
	int totalHoleCount = 0;

	// 面積 (mm^2)。部品占有面積は回転後のバウンディングサイズの単純合算
	// (部品同士が重なっていてもそのまま加算する概算値)。
	double boardAreaMm2 = 0.0;
	double occupiedAreaMm2 = 0.0;
	double occupancyRatio = 0.0;  // occupiedAreaMm2 / boardAreaMm2 (基板面積0なら0)

	int placementCount = 0;
	int wireCount = 0;
};

// BOM (部品表) の1行。部品 (ライブラリ+部品ID) と値が同じ配置をまとめたもの。
struct BomRow {
	QString refDesList;    // "R1, R2, R3" のように連結したもの
	QString partName;
	QString value;
	QString libraryName;
	int quantity = 0;
};

class StatsEngine {
public:
	BoardStats compute(const Document &document, LibraryManager *libraryManager) const;

	// 部品+値でグループ化した BOM を、配置順に最初に現れた組み合わせの順で返す。
	QVector<BomRow> computeBom(const Document &document, LibraryManager *libraryManager) const;

	// ヘッダ行付き CSV (UTF-8) に変換する。
	static QString bomToCsv(const QVector<BomRow> &rows);
	// Excel 等での文字化けを避けるため UTF-8 BOM 付きで書き出す。
	static bool saveBomCsv(const QVector<BomRow> &rows, const QString &filePath);
};
