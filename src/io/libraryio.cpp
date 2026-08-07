#include "libraryio.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>

#include "boardio.h"
#include "jsonutil.h"
#include "partio.h"
#include "zipio.h"

namespace libraryio {

namespace {

QString derivativePolicyToString(DerivativePolicy p) {
	switch (p) {
	case DerivativePolicy::Any:
		return QStringLiteral("any");
	case DerivativePolicy::MustMatchSame:
		return QStringLiteral("mustMatchSame");
	case DerivativePolicy::NcFamilyOnly:
		return QStringLiteral("ncFamilyOnly");
	}
	return QStringLiteral("any");
}

DerivativePolicy derivativePolicyFromString(const QString &s) {
	if (s == QStringLiteral("mustMatchSame")) return DerivativePolicy::MustMatchSame;
	if (s == QStringLiteral("ncFamilyOnly")) return DerivativePolicy::NcFamilyOnly;
	return DerivativePolicy::Any;
}

QString partSidecarPath(const QString &partId) {
	return QStringLiteral("parts/%1.part.json").arg(partId);
}
QString partArtworkPath(const QString &partId) {
	return QStringLiteral("parts/%1.png").arg(partId);
}
QString boardPath(const QString &boardId) {
	return QStringLiteral("boards/%1.bboard").arg(boardId);
}
QString categoryIconPath(const QString &categoryId) {
	return QStringLiteral("categories/%1/icon.png").arg(categoryId);
}

}  // namespace

bool writePackage(const Library &lib, const PackageSink &sink) {
	QJsonObject obj;
	obj["schema"] = QStringLiteral("boardes.library/%1").arg(SchemaVersion);
	obj["id"] = lib.id;
	obj["name"] = lib.name;
	obj["version"] = lib.version;
	obj["author"] = lib.author;
	obj["authorUrl"] = lib.authorUrl;
	obj["homepage"] = lib.homepage;
	obj["description"] = lib.description;

	QJsonObject license;
	license["kind"] = licenseKindToKey(lib.license.kind);
	license["customName"] = lib.license.customName;
	license["customUrl"] = lib.license.customUrl;
	license["hasLicenseFile"] = !lib.license.customLicenseText.isEmpty();
	obj["license"] = license;

	QJsonObject redistribution;
	redistribution["allowed"] = lib.redistribution.allowed;
	redistribution["attributionRequired"] = lib.redistribution.attributionRequired;
	redistribution["derivativePolicy"] = derivativePolicyToString(lib.redistribution.derivativePolicy);
	obj["redistribution"] = redistribution;

	obj["readOnly"] = lib.readOnly;

	if (lib.basedOn.has_value()) {
		QJsonObject based;
		based["id"] = lib.basedOn->libraryId;
		based["name"] = lib.basedOn->name;
		based["version"] = lib.basedOn->version;
		based["license"] = lib.basedOn->licenseLabel;
		obj["basedOn"] = based;
	}

	QJsonArray categoriesArr;
	for (const auto &cat : lib.categories) {
		QJsonObject c;
		c["id"] = cat.id;
		c["name"] = cat.name;
		c["order"] = cat.order;
		if (!cat.icon.isNull()) {
			const QString iconPath = categoryIconPath(cat.id);
			QByteArray png;
			{
				QBuffer buf(&png);
				buf.open(QIODevice::WriteOnly);
				cat.icon.save(&buf, "PNG");
			}
			if (!sink.write(iconPath, png)) {
				return false;
			}
			c["icon"] = iconPath;
		}
		categoriesArr.append(c);
	}
	obj["categories"] = categoriesArr;

	QJsonArray partsArr;
	for (auto it = lib.parts.constBegin(); it != lib.parts.constEnd(); ++it) {
		const auto &part = *it.value();
		const QString sidecarPath = partSidecarPath(part.id);
		const QString artworkPath = partArtworkPath(part.id);
		const QJsonObject partJson = partio::toJsonObject(part, /*embedBase64=*/false, QFileInfo(artworkPath).fileName());
		if (!sink.write(sidecarPath, QJsonDocument(partJson).toJson(QJsonDocument::Indented))) {
			return false;
		}
		if (!part.artwork.pngBytes.isEmpty() && !sink.write(artworkPath, part.artwork.pngBytes)) {
			return false;
		}
		QJsonObject p;
		p["id"] = part.id;
		p["category"] = lib.partCategory.value(part.id);
		p["file"] = sidecarPath;
		partsArr.append(p);
	}
	obj["parts"] = partsArr;

	QJsonArray boardsArr;
	for (auto it = lib.boards.constBegin(); it != lib.boards.constEnd(); ++it) {
		const auto &board = *it.value();
		const QString path = boardPath(board.id);
		if (!sink.write(path, QJsonDocument(boardio::toJsonObject(board)).toJson(QJsonDocument::Indented))) {
			return false;
		}
		QJsonObject b;
		b["id"] = board.id;
		b["file"] = path;
		boardsArr.append(b);
	}
	obj["boards"] = boardsArr;

	if (!lib.license.customLicenseText.isEmpty()) {
		if (!sink.write(QStringLiteral("LICENSE.txt"), lib.license.customLicenseText.toUtf8())) {
			return false;
		}
	}

	return sink.write(QStringLiteral("library.json"), QJsonDocument(obj).toJson(QJsonDocument::Indented));
}

std::optional<Library> readPackage(const PackageSource &source) {
	if (!source.exists(QStringLiteral("library.json"))) {
		return std::nullopt;
	}
	const QByteArray manifestBytes = source.read(QStringLiteral("library.json"));
	QJsonParseError err;
	const QJsonDocument doc = QJsonDocument::fromJson(manifestBytes, &err);
	if (err.error != QJsonParseError::NoError || !doc.isObject()) {
		return std::nullopt;
	}
	const QJsonObject obj = doc.object();

	Library lib;
	lib.id = obj["id"].toString();
	lib.name = obj["name"].toString();
	lib.version = obj["version"].toString();
	lib.author = obj["author"].toString();
	lib.authorUrl = obj["authorUrl"].toString();
	lib.homepage = obj["homepage"].toString();
	lib.description = obj["description"].toString();

	const QJsonObject license = obj["license"].toObject();
	lib.license.kind = licenseKindFromKey(license["kind"].toString());
	lib.license.customName = license["customName"].toString();
	lib.license.customUrl = license["customUrl"].toString();
	if (license["hasLicenseFile"].toBool() && source.exists(QStringLiteral("LICENSE.txt"))) {
		lib.license.customLicenseText = QString::fromUtf8(source.read(QStringLiteral("LICENSE.txt")));
	}

	const QJsonObject redistribution = obj["redistribution"].toObject();
	lib.redistribution.allowed = redistribution["allowed"].toBool();
	lib.redistribution.attributionRequired = redistribution["attributionRequired"].toBool();
	lib.redistribution.derivativePolicy = derivativePolicyFromString(redistribution["derivativePolicy"].toString());

	lib.readOnly = obj["readOnly"].toBool();

	if (obj.contains("basedOn")) {
		const QJsonObject based = obj["basedOn"].toObject();
		BasedOn b;
		b.libraryId = based["id"].toString();
		b.name = based["name"].toString();
		b.version = based["version"].toString();
		b.licenseLabel = based["license"].toString();
		lib.basedOn = b;
	}

	for (const auto &v : obj["categories"].toArray()) {
		const QJsonObject c = v.toObject();
		CategoryInfo cat;
		cat.id = c["id"].toString();
		cat.name = c["name"].toString();
		cat.order = c["order"].toInt();
		if (c.contains("icon")) {
			const QByteArray iconBytes = source.read(c["icon"].toString());
			if (!iconBytes.isEmpty()) {
				cat.icon.loadFromData(iconBytes);
			}
		}
		lib.categories.append(cat);
	}

	partio::ArtworkResolver resolver = [&source](const QString &relPathFromPart) -> QByteArray {
		// resolver は part.json からの相対パス (ファイル名のみ) で呼ばれるので、
		// parts/ ディレクトリを補って解決する。
		return source.read(QStringLiteral("parts/") + relPathFromPart);
	};

	for (const auto &v : obj["parts"].toArray()) {
		const QJsonObject p = v.toObject();
		const QString file = p["file"].toString();
		if (!source.exists(file)) {
			continue;
		}
		QJsonParseError perr;
		const QJsonDocument partDoc = QJsonDocument::fromJson(source.read(file), &perr);
		if (perr.error != QJsonParseError::NoError || !partDoc.isObject()) {
			continue;
		}
		auto part = std::make_shared<Part>(partio::fromJsonObject(partDoc.object(), resolver));
		lib.parts.insert(part->id, part);
		lib.partCategory.insert(part->id, p["category"].toString());
	}

	for (const auto &v : obj["boards"].toArray()) {
		const QJsonObject b = v.toObject();
		const QString file = b["file"].toString();
		if (!source.exists(file)) {
			continue;
		}
		QJsonParseError berr;
		const QJsonDocument boardDoc = QJsonDocument::fromJson(source.read(file), &berr);
		if (berr.error != QJsonParseError::NoError || !boardDoc.isObject()) {
			continue;
		}
		auto board = std::make_shared<BoardSpec>(boardio::fromJsonObject(boardDoc.object()));
		lib.boards.insert(board->id, board);
	}

	return lib;
}

bool saveToDirectory(const Library &lib, const QString &dirPath) {
	QDir().mkpath(dirPath);
	PackageSink sink;
	sink.write = [&dirPath](const QString &relPath, const QByteArray &data) -> bool {
		const QString fullPath = dirPath + QLatin1Char('/') + relPath;
		QDir().mkpath(QFileInfo(fullPath).absolutePath());
		QFile f(fullPath);
		if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
			return false;
		}
		return f.write(data) == data.size();
	};
	return writePackage(lib, sink);
}

std::optional<Library> loadFromDirectory(const QString &dirPath) {
	PackageSource source;
	source.exists = [&dirPath](const QString &relPath) -> bool {
		return QFileInfo::exists(dirPath + QLatin1Char('/') + relPath);
	};
	source.read = [&dirPath](const QString &relPath) -> QByteArray {
		QFile f(dirPath + QLatin1Char('/') + relPath);
		if (!f.open(QIODevice::ReadOnly)) {
			return {};
		}
		return f.readAll();
	};
	return readPackage(source);
}

bool exportToBlib(const Library &lib, const QString &blibFilePath) {
	ZipWriter writer;
	PackageSink sink;
	sink.write = [&writer](const QString &relPath, const QByteArray &data) -> bool {
		return writer.addFile(relPath, data);
	};
	if (!writePackage(lib, sink)) {
		return false;
	}
	const QByteArray zipBytes = writer.finish();
	if (zipBytes.isEmpty() || !writer.isValid()) {
		return false;
	}
	QFile f(blibFilePath);
	if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		return false;
	}
	return f.write(zipBytes) == zipBytes.size();
}

std::optional<Library> importFromBlib(const QString &blibFilePath) {
	ZipReader reader(blibFilePath);
	if (!reader.isValid()) {
		return std::nullopt;
	}
	PackageSource source;
	source.exists = [&reader](const QString &relPath) -> bool { return reader.contains(relPath); };
	source.read = [&reader](const QString &relPath) -> QByteArray { return reader.read(relPath); };
	return readPackage(source);
}

}  // namespace libraryio
