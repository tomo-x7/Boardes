#pragma once

#include <QPoint>
#include <QString>
#include <QVector>

#include "../core/geometry.h"

class Document;
class LibraryManager;

enum class DrcSeverity {
	Warning,
	Error,
};

struct DrcFinding {
	DrcSeverity severity = DrcSeverity::Warning;
	QString ruleId;   // "unconnected-pin" など、安定した識別子
	QString message;  // 一覧表示用の日本語メッセージ
	QPoint pos;       // ジャンプ先 (単位系)
	Side side = Side::Front;
	QString relatedPlacementUuid;  // 空も可
	QString relatedWireUuid;       // 空も可
};

// PasS には無い機能。以下の7ルールを editing のたびに増分ではなく毎回まとめて検査する
// (基板規模的に全件走査で十分高速なため)。
//   1. 未接続ピン (そのピンだけのネット)                          警告
//   2. 2部品が同じ穴を共有                                        エラー
//   3. 部品アウトラインの重なり (ICソケット+IC は正常なので)      警告
//   4. ピン/配線が基板の外形の外                                  エラー
//   5. 同じ面の裸線どうしが格子点以外で交差 (ショート)            エラー
//   6. 同じ穴に異なるドリル径のピン                                警告
//   7. 両面基板で、表面配線の端点にピンもスルーホールも無い        警告
class DrcEngine {
public:
	QVector<DrcFinding> run(const Document &document, LibraryManager *libraryManager) const;
};
