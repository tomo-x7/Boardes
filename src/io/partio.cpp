#include "partio.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>

#include "jsonutil.h"

namespace partio {

QString defaultArtworkFileName(const Part &part) {
	return part.id + QStringLiteral(".png");
}

QJsonObject toJsonObject(const Part &part, bool embedBase64, const QString &artworkFileName) {
	QJsonObject obj;
	obj["schema"] = QStringLiteral("boardes.part/%1").arg(SchemaVersion);
	obj["id"] = part.id;
	obj["name"] = part.name;
	if (!part.description.isEmpty()) {
		obj["description"] = part.description;
	}
	obj["kind"] = partKindToString(part.kind);
	obj["refPrefix"] = part.refPrefix;
	obj["keywords"] = QJsonArray::fromStringList(part.keywords);
	obj["size"] = jsonutil::fromSize(part.size());
	obj["outline"] = jsonutil::fromRect(part.outline);
	if (part.anchorExplicit) {
		obj["anchor"] = jsonutil::fromPoint(part.anchor);
	}

	QJsonObject artworkObj;
	artworkObj["type"] = QStringLiteral("raster");
	if (part.artwork.chromaKey.has_value()) {
		artworkObj["chromaKey"] = jsonutil::fromColor(*part.artwork.chromaKey);
	}
	if (embedBase64) {
		artworkObj["encoding"] = QStringLiteral("base64");
		artworkObj["data"] = QString::fromLatin1(part.artwork.pngBytes.toBase64());
	} else {
		artworkObj["encoding"] = QStringLiteral("file");
		artworkObj["file"] = artworkFileName.isEmpty() ? defaultArtworkFileName(part) : artworkFileName;
	}
	obj["artwork"] = artworkObj;

	QJsonArray pinsArr;
	for (const auto &pin : part.pins) {
		QJsonObject p;
		p["no"] = pin.number;
		p["pos"] = jsonutil::fromPoint(pin.pos);
		p["drill"] = pin.drill;
		if (!pin.name.isEmpty()) {
			p["name"] = pin.name;
		}
		pinsArr.append(p);
	}
	obj["pins"] = pinsArr;

	return obj;
}

Part fromJsonObject(const QJsonObject &obj, const ArtworkResolver &resolver) {
	Part part;
	part.id = obj["id"].toString();
	part.name = obj["name"].toString();
	part.description = obj["description"].toString();
	part.kind = partKindFromString(obj["kind"].toString());
	part.refPrefix = obj["refPrefix"].toString();
	for (const auto &v : obj["keywords"].toArray()) {
		part.keywords.append(v.toString());
	}
	part.outline = jsonutil::toRect(obj["outline"].toArray());
	if (obj.contains("anchor")) {
		part.anchor = jsonutil::toPoint(obj["anchor"].toArray());
		part.anchorExplicit = true;
	}

	const QJsonObject artworkObj = obj["artwork"].toObject();
	const QString encoding = artworkObj["encoding"].toString();
	QByteArray pngBytes;
	if (encoding == QStringLiteral("base64")) {
		pngBytes = QByteArray::fromBase64(artworkObj["data"].toString().toLatin1());
	} else if (encoding == QStringLiteral("file")) {
		if (resolver) {
			pngBytes = resolver(artworkObj["file"].toString());
		}
	}
	if (!pngBytes.isEmpty()) {
		part.artwork.pngBytes = pngBytes;
		part.artwork.decodeToImage();
	}
	if (artworkObj.contains("chromaKey")) {
		part.artwork.chromaKey = jsonutil::toColor(artworkObj["chromaKey"].toString());
	}

	for (const auto &v : obj["pins"].toArray()) {
		const QJsonObject p = v.toObject();
		Pin pin;
		pin.number = p["no"].toInt();
		pin.pos = jsonutil::toPoint(p["pos"].toArray());
		pin.drill = p["drill"].toInt();
		pin.name = p["name"].toString();
		part.pins.append(pin);
	}

	return part;
}

LoadResult validateJson(const QJsonObject &obj, const QString &path) {
	if (auto r = validate::schemaField(obj, QStringLiteral("part"), SchemaVersion); !r) {
		return r;
	}
	if (auto r = validate::nonEmptyString(obj["id"], path + QStringLiteral(".id")); !r) {
		return r;
	}
	static const QSet<QString> kKnownKinds = {QStringLiteral("normal"),      QStringLiteral("text"),
											  QStringLiteral("toolFillTop"), QStringLiteral("toolFillBottom"),
											  QStringLiteral("toolThruHole"), QStringLiteral("drillHole")};
	if (!kKnownKinds.contains(obj["kind"].toString())) {
		return LoadResult::failure(QStringLiteral("ファイルが壊れています"),
									QStringLiteral("%1.kind が未知の値です").arg(path));
	}

	if (auto r = validate::object(obj["artwork"], path + QStringLiteral(".artwork")); !r) return r;
	const QJsonObject artwork = obj["artwork"].toObject();
	const QString encoding = artwork["encoding"].toString();
	if (encoding != QStringLiteral("file") && encoding != QStringLiteral("base64")) {
		return LoadResult::failure(QStringLiteral("ファイルが壊れています"),
									QStringLiteral("%1.artwork.encoding が file/base64 ではありません").arg(path));
	}
	if (encoding == QStringLiteral("base64")) {
		if (auto r = validate::pngBase64(artwork["data"].toString(), path + QStringLiteral(".artwork.data")); !r) {
			return r;
		}
	} else if (artwork["file"].toString().isEmpty()) {
		return LoadResult::failure(QStringLiteral("ファイルが壊れています"),
									QStringLiteral("%1.artwork.file が空です").arg(path));
	}

	if (obj.contains("anchor")) {
		if (auto r = validate::intPair(obj["anchor"], path + QStringLiteral(".anchor")); !r) return r;
	}

	if (obj.contains("pins")) {
		if (auto r = validate::array(obj["pins"], path + QStringLiteral(".pins")); !r) return r;
		const QJsonArray pins = obj["pins"].toArray();
		for (int i = 0; i < pins.size(); ++i) {
			const QString pinPath = QStringLiteral("%1.pins[%2]").arg(path).arg(i);
			if (auto r = validate::object(pins[i], pinPath); !r) return r;
			const QJsonObject p = pins[i].toObject();
			if (auto r = validate::intPair(p["pos"], pinPath + QStringLiteral(".pos")); !r) return r;
			if (p["no"].toInt(-1) < 0) {
				return LoadResult::failure(QStringLiteral("ファイルが壊れています"),
											QStringLiteral("%1.no が負です").arg(pinPath));
			}
			if (p["drill"].toInt(-1) < 0) {
				return LoadResult::failure(QStringLiteral("ファイルが壊れています"),
											QStringLiteral("%1.drill が負です").arg(pinPath));
			}
		}
	}

	return LoadResult::success();
}

bool saveEmbedded(const Part &part, const QString &bpartFilePath) {
	const QJsonObject obj = toJsonObject(part, /*embedBase64=*/true);
	QFile f(bpartFilePath);
	if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		return false;
	}
	f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
	return true;
}

std::optional<Part> loadEmbedded(const QString &bpartFilePath, LoadResult *errorOut) {
	QFile f(bpartFilePath);
	if (!f.open(QIODevice::ReadOnly)) {
		if (errorOut) *errorOut = LoadResult::failure(QStringLiteral("ファイルを開けません"), bpartFilePath);
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
	if (const auto r = validateJson(doc.object(), QStringLiteral("part")); !r) {
		if (errorOut) *errorOut = r;
		return std::nullopt;
	}
	Part part = fromJsonObject(doc.object(), nullptr);
	if (part.artwork.image.isNull()) {
		if (errorOut) {
			*errorOut = LoadResult::failure(QStringLiteral("ファイルが壊れています"),
											QStringLiteral("part.artwork の画像をデコードできませんでした"));
		}
		return std::nullopt;
	}
	return part;
}

bool saveSidecar(const Part &part, const QString &jsonFilePath) {
	const QFileInfo fi(jsonFilePath);
	const QString artworkFileName = defaultArtworkFileName(part);
	const QString artworkPath = fi.absolutePath() + QLatin1Char('/') + artworkFileName;

	if (!part.artwork.pngBytes.isEmpty()) {
		QFile img(artworkPath);
		if (!img.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
			return false;
		}
		img.write(part.artwork.pngBytes);
	}

	const QJsonObject obj = toJsonObject(part, /*embedBase64=*/false, artworkFileName);
	QFile f(jsonFilePath);
	if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		return false;
	}
	f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
	return true;
}

std::optional<Part> loadSidecar(const QString &jsonFilePath, LoadResult *errorOut) {
	QFile f(jsonFilePath);
	if (!f.open(QIODevice::ReadOnly)) {
		if (errorOut) *errorOut = LoadResult::failure(QStringLiteral("ファイルを開けません"), jsonFilePath);
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
	if (const auto r = validateJson(doc.object(), QStringLiteral("part")); !r) {
		if (errorOut) *errorOut = r;
		return std::nullopt;
	}
	const QFileInfo fi(jsonFilePath);
	const QString dir = fi.absolutePath();
	ArtworkResolver resolver = [dir](const QString &relativePath) -> QByteArray {
		QFile img(dir + QLatin1Char('/') + relativePath);
		if (!img.open(QIODevice::ReadOnly)) {
			return {};
		}
		return img.readAll();
	};
	Part part = fromJsonObject(doc.object(), resolver);
	if (part.artwork.image.isNull()) {
		if (errorOut) {
			*errorOut = LoadResult::failure(
				QStringLiteral("ファイルが壊れています"),
				QStringLiteral("サイドカー画像が見つからないかデコードできませんでした"));
		}
		return std::nullopt;
	}
	return part;
}

}  // namespace partio
