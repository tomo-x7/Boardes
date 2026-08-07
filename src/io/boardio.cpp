#include "boardio.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

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

bool save(const BoardSpec &board, const QString &filePath) {
	QFile f(filePath);
	if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		return false;
	}
	f.write(QJsonDocument(toJsonObject(board)).toJson(QJsonDocument::Indented));
	return true;
}

std::optional<BoardSpec> load(const QString &filePath) {
	QFile f(filePath);
	if (!f.open(QIODevice::ReadOnly)) {
		return std::nullopt;
	}
	QJsonParseError err;
	const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
	if (err.error != QJsonParseError::NoError || !doc.isObject()) {
		return std::nullopt;
	}
	return fromJsonObject(doc.object());
}

}  // namespace boardio
