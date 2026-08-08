#pragma once

#include <QColor>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QSet>
#include <QString>
#include <QStringList>

// 内部ファイル (.boardes/.bpkg/.blib/library.json/.bpart/.part.json/.bboard) の
// 読み込み結果。形式が合っていなければ黙って部分的に読むのではなく、はっきり
// 失敗として返す (Phase 17)。
struct LoadResult {
	bool ok = false;
	QString summary;       // 「ファイルが壊れています」等、ダイアログのタイトル行
	QString detail;        // 「placements[3].pos が長さ2の数値配列ではありません」等、どこが問題かの詳細
	QStringList warnings;  // 致命的ではない問題 (読み込みは成功する)

	explicit operator bool() const {
		return ok;
	}

	static LoadResult success() {
		LoadResult r;
		r.ok = true;
		return r;
	}
	static LoadResult failure(const QString &summary, const QString &detail) {
		LoadResult r;
		r.ok = false;
		r.summary = summary;
		r.detail = detail;
		return r;
	}
};

// 5つの IO (documentio/boardio/partio/libraryio/zipio) すべてが使う共通の検証ヘルパー。
// 同じ検証ロジックを何度も書かないためのもの。
namespace validate {

// "boardes.<kind>/N" の形と、種別・バージョンを検証する。
inline LoadResult schemaField(const QJsonObject &root, const QString &expectedKind, int maxVersion) {
	if (!root.contains(QStringLiteral("schema"))) {
		return LoadResult::failure(QStringLiteral("ファイルが壊れています"), QStringLiteral("schema がありません"));
	}
	const QString schema = root[QStringLiteral("schema")].toString();
	const QString prefix = QStringLiteral("boardes.") + expectedKind + QLatin1Char('/');
	if (!schema.startsWith(prefix)) {
		return LoadResult::failure(QStringLiteral("ファイルが壊れています"),
									QStringLiteral("schema が不正です (種別が一致しません): %1").arg(schema));
	}
	bool ok = false;
	const int version = schema.mid(prefix.size()).toInt(&ok);
	if (!ok) {
		return LoadResult::failure(QStringLiteral("ファイルが壊れています"),
									QStringLiteral("schema のバージョン番号を解釈できません: %1").arg(schema));
	}
	if (version > maxVersion) {
		return LoadResult::failure(
			QStringLiteral("新しいバージョンの Boardes で作られています"),
			QStringLiteral("schema バージョン %1 はこの Boardes (対応 %2 まで) より新しいです").arg(version).arg(maxVersion));
	}
	return LoadResult::success();
}

inline LoadResult object(const QJsonValue &v, const QString &path) {
	if (!v.isObject()) {
		return LoadResult::failure(QStringLiteral("ファイルが壊れています"),
									QStringLiteral("%1: オブジェクトではありません").arg(path));
	}
	return LoadResult::success();
}

inline LoadResult array(const QJsonValue &v, const QString &path) {
	if (!v.isArray()) {
		return LoadResult::failure(QStringLiteral("ファイルが壊れています"),
									QStringLiteral("%1: 配列ではありません").arg(path));
	}
	return LoadResult::success();
}

inline LoadResult nonEmptyString(const QJsonValue &v, const QString &path) {
	if (!v.isString() || v.toString().isEmpty()) {
		return LoadResult::failure(QStringLiteral("ファイルが壊れています"),
									QStringLiteral("%1: 空でない文字列ではありません").arg(path));
	}
	return LoadResult::success();
}

// 長さ2の数値配列 ([x, y] や [w, h])。
inline LoadResult intPair(const QJsonValue &v, const QString &path) {
	if (!v.isArray()) {
		return LoadResult::failure(QStringLiteral("ファイルが壊れています"),
									QStringLiteral("%1: 長さ2の数値配列ではありません").arg(path));
	}
	const QJsonArray a = v.toArray();
	if (a.size() != 2 || !a[0].isDouble() || !a[1].isDouble()) {
		return LoadResult::failure(QStringLiteral("ファイルが壊れています"),
									QStringLiteral("%1: 長さ2の数値配列ではありません").arg(path));
	}
	return LoadResult::success();
}

inline LoadResult uniqueIds(const QStringList &ids, const QString &path) {
	QSet<QString> seen;
	for (const QString &id : ids) {
		if (id.isEmpty()) {
			return LoadResult::failure(QStringLiteral("ファイルが壊れています"),
										QStringLiteral("%1: id が空の要素があります").arg(path));
		}
		if (seen.contains(id)) {
			return LoadResult::failure(QStringLiteral("ファイルが壊れています"),
										QStringLiteral("%1: id が重複しています (%2)").arg(path, id));
		}
		seen.insert(id);
	}
	return LoadResult::success();
}

// base64 文字列をデコードして画像として読めるかどうか。
inline LoadResult pngBase64(const QString &base64, const QString &path) {
	const QByteArray bytes = QByteArray::fromBase64(base64.toLatin1());
	if (bytes.isEmpty()) {
		return LoadResult::failure(QStringLiteral("ファイルが壊れています"),
									QStringLiteral("%1: base64 データが空か不正です").arg(path));
	}
	QImage img;
	if (!img.loadFromData(bytes)) {
		return LoadResult::failure(QStringLiteral("ファイルが壊れています"),
									QStringLiteral("%1: 画像としてデコードできません").arg(path));
	}
	return LoadResult::success();
}

// zip エントリ名が ".." や絶対パスを含んでいないか (zip slip 対策)。
inline LoadResult safeZipEntryName(const QString &name) {
	if (name.isEmpty() || name.startsWith(QLatin1Char('/')) || name.contains(QStringLiteral("..")) ||
		(name.size() >= 2 && name[1] == QLatin1Char(':'))) {  // "C:\..." 形式のドライブレターも拒否
		return LoadResult::failure(QStringLiteral("ファイルが壊れています"),
									QStringLiteral("不正なエントリ名です: %1").arg(name));
	}
	return LoadResult::success();
}

}  // namespace validate
