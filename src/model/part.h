#pragma once

#include <QByteArray>
#include <QColor>
#include <QImage>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QVector>
#include <optional>

// 部品の種類。表示・配置ロジックの分岐に使う。
enum class PartKind {
	Normal,          // 通常の電子部品
	Text,            // テキスト部品 (コメント)
	ToolFillTop,     // 表面塗りつぶし
	ToolFillBottom,  // 裏面塗りつぶし
	ToolThruHole,    // スルーホール (表裏接続)
	DrillHole,       // 単純なドリル穴 (ピン番号なし)
};

QString partKindToString(PartKind kind);
PartKind partKindFromString(const QString &s);

// 部品のピン。pos は部品原点からの相対座標 (単位系)。
struct Pin {
	int number = 0;    // 0 = 番号なし (単純なドリル穴として扱う)
	QPoint pos;
	int drill = 0;     // 0 = 既定ドリル径。それ以外は「code * 0.1mm」
	QString name;      // 任意のピン名 (省略可)

	bool hasNumber() const {
		return number > 0;
	}
};

// 部品の見た目 (ラスタ画像)。
//
// `image` は常に「そのまま描画できる」状態 (背景が透過済みなど) で保持する。
// `pngBytes` はシリアライズ用の正規バイト列で、encodeFromImage()/decodeToImage() で
// 相互に変換する。chromaKey は「どの色を背景として透過したか」を記録するメタデータで、
// 部品エディタでの再処理に使う (レンダリングは image の alpha をそのまま使うので必須ではない)。
struct Artwork {
	QImage image;
	QByteArray pngBytes;
	std::optional<QColor> chromaKey;

	bool isNull() const {
		return image.isNull();
	}

	// image から pngBytes を再生成する。
	void encodeFromImage();
	// pngBytes から image を再構築する (成功したら true)。
	bool decodeToImage();

	// source の chromaKey に一致する画素を透過にした Artwork を作る。
	static Artwork fromChromaKeyed(QImage source, QColor key);
	// 透過処理をせずそのまま使う Artwork を作る (ユーザーが用意した PNG など)。
	static Artwork fromImageAsIs(QImage source);
};

// 部品定義。ライブラリに格納される不変データ。
class Part {
public:
	QString id;
	QString name;
	QString description;
	QStringList keywords;
	PartKind kind = PartKind::Normal;
	QString refPrefix;  // "R" / "C" / "IC" など、自動リファレンス採番の接頭辞
	Artwork artwork;
	QVector<Pin> pins;
	QRect outline;  // アウトライン表示用の矩形 (単位系)。artwork のサイズと独立に持てる

	QSize size() const {
		return artwork.isNull() ? outline.size() : artwork.image.size();
	}

	// キーワード検索 (部分一致、大小無視)。
	bool matchesQuery(const QString &query) const;
};
