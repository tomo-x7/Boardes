#include "partio.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>

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

bool saveEmbedded(const Part &part, const QString &bpartFilePath) {
	const QJsonObject obj = toJsonObject(part, /*embedBase64=*/true);
	QFile f(bpartFilePath);
	if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		return false;
	}
	f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
	return true;
}

std::optional<Part> loadEmbedded(const QString &bpartFilePath) {
	QFile f(bpartFilePath);
	if (!f.open(QIODevice::ReadOnly)) {
		return std::nullopt;
	}
	QJsonParseError err;
	const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
	if (err.error != QJsonParseError::NoError || !doc.isObject()) {
		return std::nullopt;
	}
	return fromJsonObject(doc.object(), nullptr);
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

std::optional<Part> loadSidecar(const QString &jsonFilePath) {
	QFile f(jsonFilePath);
	if (!f.open(QIODevice::ReadOnly)) {
		return std::nullopt;
	}
	QJsonParseError err;
	const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
	if (err.error != QJsonParseError::NoError || !doc.isObject()) {
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
	return fromJsonObject(doc.object(), resolver);
}

}  // namespace partio
