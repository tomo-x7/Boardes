#include "boardio.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>

#include "jsonutil.h"

namespace boardio {

namespace {

QJsonObject artworkToJson(const Artwork &art) {
	QJsonObject o;
	o["encoding"] = QStringLiteral("base64");
	o["data"] = QString::fromLatin1(art.pngBytes.toBase64());
	if (art.chromaKey.has_value()) {
		o["chromaKey"] = jsonutil::fromColor(*art.chromaKey);
	}
	return o;
}

Artwork artworkFromJson(const QJsonObject &o) {
	Artwork art;
	art.pngBytes = QByteArray::fromBase64(o["data"].toString().toLatin1());
	art.decodeToImage();
	if (o.contains("chromaKey")) {
		art.chromaKey = jsonutil::toColor(o["chromaKey"].toString());
	}
	return art;
}

}  // namespace

QJsonObject toJsonObject(const BoardSpec &board) {
	QJsonObject obj;
	obj["schema"] = QStringLiteral("boardes.board/%1").arg(SchemaVersion);
	obj["id"] = board.id;
	obj["name"] = board.name;
	obj["size"] = jsonutil::fromSize(board.size);

	QJsonObject grid;
	grid["cols"] = board.cols;
	grid["rows"] = board.rows;
	grid["pitch"] = board.pitch;
	grid["origin"] = jsonutil::fromPoint(board.origin);
	grid["absentHoles"] = jsonutil::fromPointList(board.absentHoles);
	obj["grid"] = grid;

	QJsonObject pad;
	pad["shape"] = padShapeToString(board.padShape);
	pad["padDiameter"] = board.padDiameter;
	pad["holeDiameter"] = board.holeDiameter;
	obj["pad"] = pad;

	QJsonObject copper;
	copper["type"] = copperPatternToString(board.copper);
	copper["doubleSided"] = board.doubleSided;
	copper["stripBreaks"] = jsonutil::fromPointList(board.stripBreaks);
	obj["copper"] = copper;

	QJsonObject colors;
	colors["substrate"] = jsonutil::fromColor(board.substrateColor);
	colors["pad"] = jsonutil::fromColor(board.padColor);
	colors["copper"] = jsonutil::fromColor(board.copperColor);
	obj["colors"] = colors;

	QJsonArray mounting;
	for (const auto &mh : board.mountingHoles) {
		QJsonObject m;
		m["pos"] = jsonutil::fromPoint(mh.first);
		m["diameter"] = mh.second;
		mounting.append(m);
	}
	obj["mountingHoles"] = mounting;

	if (board.backgroundFront.has_value() || board.backgroundBack.has_value()) {
		QJsonObject bg;
		if (board.backgroundFront.has_value()) {
			bg["front"] = artworkToJson(*board.backgroundFront);
		}
		if (board.backgroundBack.has_value()) {
			bg["back"] = artworkToJson(*board.backgroundBack);
		}
		obj["background"] = bg;
	}

	if (board.outlineRect.has_value()) {
		obj["outlineRect"] = jsonutil::fromRect(*board.outlineRect);
	}

	return obj;
}

BoardSpec fromJsonObject(const QJsonObject &obj) {
	BoardSpec board;
	board.id = obj["id"].toString();
	board.name = obj["name"].toString();
	board.size = jsonutil::toSize(obj["size"].toArray());

	const QJsonObject grid = obj["grid"].toObject();
	board.cols = grid["cols"].toInt();
	board.rows = grid["rows"].toInt();
	board.pitch = grid["pitch"].toInt(10);
	board.origin = jsonutil::toPoint(grid["origin"].toArray());
	board.absentHoles = jsonutil::toPointList(grid["absentHoles"].toArray());

	const QJsonObject pad = obj["pad"].toObject();
	board.padShape = padShapeFromString(pad["shape"].toString());
	board.padDiameter = pad["padDiameter"].toInt(6);
	board.holeDiameter = pad["holeDiameter"].toInt(3);

	const QJsonObject copper = obj["copper"].toObject();
	board.copper = copperPatternFromString(copper["type"].toString());
	board.doubleSided = copper["doubleSided"].toBool();
	board.stripBreaks = jsonutil::toPointList(copper["stripBreaks"].toArray());

	const QJsonObject colors = obj["colors"].toObject();
	board.substrateColor = jsonutil::toColor(colors["substrate"].toString(), boarddefaults::Substrate);
	board.padColor = jsonutil::toColor(colors["pad"].toString(), boarddefaults::Pad);
	board.copperColor = jsonutil::toColor(colors["copper"].toString(), boarddefaults::Copper);

	for (const auto &v : obj["mountingHoles"].toArray()) {
		const QJsonObject m = v.toObject();
		board.mountingHoles.append({jsonutil::toPoint(m["pos"].toArray()), m["diameter"].toInt()});
	}

	if (obj.contains("background")) {
		const QJsonObject bg = obj["background"].toObject();
		if (bg.contains("front")) {
			board.backgroundFront = artworkFromJson(bg["front"].toObject());
		}
		if (bg.contains("back")) {
			board.backgroundBack = artworkFromJson(bg["back"].toObject());
		}
	}

	if (obj.contains("outlineRect") && obj["outlineRect"].isArray()) {
		board.outlineRect = jsonutil::toRect(obj["outlineRect"].toArray());
	}

	return board;
}

namespace {
LoadResult validateColor(const QJsonValue &v, const QString &path) {
	QColor c(v.toString());
	if (!c.isValid()) {
		return LoadResult::failure(QStringLiteral("ファイルが壊れています"),
									QStringLiteral("%1: 色として解釈できません (%2)").arg(path, v.toString()));
	}
	return LoadResult::success();
}
}  // namespace

LoadResult validateJson(const QJsonObject &obj, const QString &path) {
	if (auto r = validate::schemaField(obj, QStringLiteral("board"), SchemaVersion); !r) {
		return r;
	}
	if (auto r = validate::intPair(obj["size"], path + QStringLiteral(".size")); !r) {
		return r;
	}

	if (auto r = validate::object(obj["grid"], path + QStringLiteral(".grid")); !r) return r;
	const QJsonObject grid = obj["grid"].toObject();
	if (grid["cols"].toInt(-1) < 0 || grid["rows"].toInt(-1) < 0) {
		return LoadResult::failure(QStringLiteral("ファイルが壊れています"),
									QStringLiteral("%1.grid.cols/rows が不正です").arg(path));
	}
	if (grid["pitch"].toInt(0) < 1) {
		return LoadResult::failure(QStringLiteral("ファイルが壊れています"),
									QStringLiteral("%1.grid.pitch は1以上である必要があります").arg(path));
	}
	if (auto r = validate::intPair(grid["origin"], path + QStringLiteral(".grid.origin")); !r) return r;

	if (auto r = validate::object(obj["pad"], path + QStringLiteral(".pad")); !r) return r;
	static const QSet<QString> kKnownPadShapes = {QStringLiteral("none"), QStringLiteral("round"),
												  QStringLiteral("square")};
	if (!kKnownPadShapes.contains(obj["pad"].toObject()["shape"].toString())) {
		return LoadResult::failure(QStringLiteral("ファイルが壊れています"),
									QStringLiteral("%1.pad.shape が未知の値です").arg(path));
	}

	if (auto r = validate::object(obj["copper"], path + QStringLiteral(".copper")); !r) return r;
	static const QSet<QString> kKnownCopperTypes = {QStringLiteral("none"), QStringLiteral("padPerHole"),
													QStringLiteral("stripHorizontal"),
													QStringLiteral("stripVertical")};
	if (!kKnownCopperTypes.contains(obj["copper"].toObject()["type"].toString())) {
		return LoadResult::failure(QStringLiteral("ファイルが壊れています"),
									QStringLiteral("%1.copper.type が未知の値です").arg(path));
	}

	if (obj.contains("colors")) {
		if (auto r = validate::object(obj["colors"], path + QStringLiteral(".colors")); !r) return r;
		const QJsonObject colors = obj["colors"].toObject();
		for (const QString &key : {QStringLiteral("substrate"), QStringLiteral("pad"), QStringLiteral("copper")}) {
			if (colors.contains(key)) {
				if (auto r = validateColor(colors[key], path + QStringLiteral(".colors.") + key); !r) return r;
			}
		}
	}

	if (obj.contains("background")) {
		if (auto r = validate::object(obj["background"], path + QStringLiteral(".background")); !r) return r;
		const QJsonObject bg = obj["background"].toObject();
		for (const QString &side : {QStringLiteral("front"), QStringLiteral("back")}) {
			if (!bg.contains(side)) continue;
			if (auto r = validate::object(bg[side], path + QStringLiteral(".background.") + side); !r) return r;
			const QJsonObject art = bg[side].toObject();
			if (auto r = validate::pngBase64(art["data"].toString(), path + QStringLiteral(".background.") + side);
				!r) {
				return r;
			}
		}
	}

	return LoadResult::success();
}

bool save(const BoardSpec &board, const QString &filePath) {
	QFile f(filePath);
	if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		return false;
	}
	f.write(QJsonDocument(toJsonObject(board)).toJson(QJsonDocument::Indented));
	return true;
}

std::optional<BoardSpec> load(const QString &filePath, LoadResult *errorOut) {
	QFile f(filePath);
	if (!f.open(QIODevice::ReadOnly)) {
		if (errorOut) {
			*errorOut = LoadResult::failure(QStringLiteral("ファイルを開けません"),
											QStringLiteral("ファイルを読み込めませんでした: %1").arg(filePath));
		}
		return std::nullopt;
	}
	QJsonParseError err;
	const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
	if (err.error != QJsonParseError::NoError || !doc.isObject()) {
		if (errorOut) {
			*errorOut = LoadResult::failure(QStringLiteral("ファイルが壊れています"),
											QStringLiteral("JSON として解析できません: %1").arg(err.errorString()));
		}
		return std::nullopt;
	}
	if (const auto r = validateJson(doc.object(), QStringLiteral("board")); !r) {
		if (errorOut) *errorOut = r;
		return std::nullopt;
	}
	return fromJsonObject(doc.object());
}

}  // namespace boardio
