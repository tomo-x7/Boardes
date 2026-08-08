#include "documentio.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>

#include "boardio.h"
#include "jsonutil.h"
#include "libraryio.h"
#include "zipio.h"

namespace documentio {

namespace {

QString sideToString(Side s) {
	return s == Side::Back ? QStringLiteral("back") : QStringLiteral("front");
}
Side sideFromString(const QString &s) {
	return s == QStringLiteral("back") ? Side::Back : Side::Front;
}

QJsonObject placementToJson(const Placement &p) {
	QJsonObject o;
	o["uuid"] = p.uuid;
	o["lib"] = p.libraryId;
	o["part"] = p.partId;
	o["pos"] = jsonutil::fromPoint(p.pos);
	o["rot"] = static_cast<int>(p.rot);
	o["side"] = sideToString(p.side);
	o["refDes"] = p.refDes;
	o["value"] = p.value;
	o["labels"] = p.labelsVisible;
	o["visible"] = p.visible;
	o["z"] = p.z;
	return o;
}

Placement placementFromJson(const QJsonObject &o) {
	Placement p;
	p.uuid = o["uuid"].toString();
	p.libraryId = o["lib"].toString();
	p.partId = o["part"].toString();
	p.pos = jsonutil::toPoint(o["pos"].toArray());
	p.rot = static_cast<Rotation>(o["rot"].toInt());
	p.side = sideFromString(o["side"].toString());
	p.refDes = o["refDes"].toString();
	p.value = o["value"].toString();
	p.labelsVisible = o["labels"].toBool(true);
	p.visible = o["visible"].toBool(true);
	p.z = o["z"].toInt();
	return p;
}

QJsonObject wireToJson(const Wire &w) {
	QJsonObject o;
	o["uuid"] = w.uuid;
	o["layer"] = wireLayerToString(w.layer);
	o["points"] = jsonutil::fromPointList(w.points);
	o["visible"] = w.visible;
	return o;
}

Wire wireFromJson(const QJsonObject &o) {
	Wire w;
	w.uuid = o["uuid"].toString();
	w.layer = wireLayerFromString(o["layer"].toString());
	w.points = jsonutil::toPointList(o["points"].toArray());
	w.visible = o["visible"].toBool(true);
	return w;
}

QJsonObject dependencyToJson(const Dependency &d) {
	QJsonObject o;
	o["libraryId"] = d.libraryId;
	o["name"] = d.name;
	o["version"] = d.version;
	o["licenseSpdx"] = d.licenseSpdx;
	o["redistributable"] = d.redistributable;
	return o;
}

Dependency dependencyFromJson(const QJsonObject &o) {
	Dependency d;
	d.libraryId = o["libraryId"].toString();
	d.name = o["name"].toString();
	d.version = o["version"].toString();
	d.licenseSpdx = o["licenseSpdx"].toString();
	d.redistributable = o["redistributable"].toBool();
	return d;
}

}  // namespace

QJsonObject toJsonObject(const Document &doc) {
	QJsonObject obj;
	obj["schema"] = QStringLiteral("boardes.document/%1").arg(SchemaVersion);
	obj["title"] = doc.title;
	obj["author"] = doc.author;
	obj["notes"] = doc.notes;
	obj["board"] = boardio::toJsonObject(doc.board);

	QJsonArray deps;
	for (const auto &d : doc.dependencies) {
		deps.append(dependencyToJson(d));
	}
	obj["dependencies"] = deps;

	QJsonArray placements;
	for (const auto &p : doc.placements) {
		placements.append(placementToJson(*p));
	}
	obj["placements"] = placements;

	QJsonArray wires;
	for (const auto &w : doc.wires) {
		wires.append(wireToJson(*w));
	}
	obj["wires"] = wires;

	return obj;
}

void fromJsonObject(const QJsonObject &obj, Document &doc) {
	doc.title = obj["title"].toString();
	doc.author = obj["author"].toString();
	doc.notes = obj["notes"].toString();
	doc.board = boardio::fromJsonObject(obj["board"].toObject());

	doc.dependencies.clear();
	for (const auto &v : obj["dependencies"].toArray()) {
		doc.dependencies.append(dependencyFromJson(v.toObject()));
	}

	doc.placements.clear();
	for (const auto &v : obj["placements"].toArray()) {
		doc.placements.append(std::make_shared<Placement>(placementFromJson(v.toObject())));
	}

	doc.wires.clear();
	for (const auto &v : obj["wires"].toArray()) {
		doc.wires.append(std::make_shared<Wire>(wireFromJson(v.toObject())));
	}
}

namespace {

LoadResult validatePlacementsArray(const QJsonArray &arr) {
	QStringList uuids;
	for (int i = 0; i < arr.size(); ++i) {
		const QString path = QStringLiteral("placements[%1]").arg(i);
		if (auto r = validate::object(arr[i], path); !r) return r;
		const QJsonObject o = arr[i].toObject();
		if (auto r = validate::nonEmptyString(o["uuid"], path + QStringLiteral(".uuid")); !r) return r;
		uuids << o["uuid"].toString();
		if (auto r = validate::nonEmptyString(o["lib"], path + QStringLiteral(".lib")); !r) return r;
		if (auto r = validate::nonEmptyString(o["part"], path + QStringLiteral(".part")); !r) return r;
		if (auto r = validate::intPair(o["pos"], path + QStringLiteral(".pos")); !r) return r;
		const int rot = o["rot"].toInt(-1);
		if (rot != 0 && rot != 90 && rot != 180 && rot != 270) {
			return LoadResult::failure(
				QStringLiteral("ファイルが壊れています"),
				QStringLiteral("%1.rot が 0/90/180/270 ではありません (値: %2)").arg(path).arg(rot));
		}
		const QString side = o["side"].toString();
		if (side != QStringLiteral("front") && side != QStringLiteral("back")) {
			return LoadResult::failure(QStringLiteral("ファイルが壊れています"),
										QStringLiteral("%1.side が front/back ではありません: %2").arg(path, side));
		}
	}
	return validate::uniqueIds(uuids, QStringLiteral("placements"));
}

LoadResult validateWiresArray(const QJsonArray &arr) {
	QStringList uuids;
	static const QSet<QString> kKnownLayers = {QStringLiteral("backBare"), QStringLiteral("frontBare"),
											   QStringLiteral("backInsulated"), QStringLiteral("frontInsulated"),
											   QStringLiteral("outline")};
	for (int i = 0; i < arr.size(); ++i) {
		const QString path = QStringLiteral("wires[%1]").arg(i);
		if (auto r = validate::object(arr[i], path); !r) return r;
		const QJsonObject o = arr[i].toObject();
		if (auto r = validate::nonEmptyString(o["uuid"], path + QStringLiteral(".uuid")); !r) return r;
		uuids << o["uuid"].toString();
		if (!kKnownLayers.contains(o["layer"].toString())) {
			return LoadResult::failure(
				QStringLiteral("ファイルが壊れています"),
				QStringLiteral("%1.layer が未知の値です: %2").arg(path, o["layer"].toString()));
		}
		if (auto r = validate::array(o["points"], path + QStringLiteral(".points")); !r) return r;
		const QJsonArray points = o["points"].toArray();
		if (points.size() < 2) {
			return LoadResult::failure(QStringLiteral("ファイルが壊れています"),
										QStringLiteral("%1.points は2点以上必要です").arg(path));
		}
		for (int pi = 0; pi < points.size(); ++pi) {
			if (auto r = validate::intPair(points[pi], QStringLiteral("%1.points[%2]").arg(path).arg(pi)); !r) {
				return r;
			}
		}
	}
	return validate::uniqueIds(uuids, QStringLiteral("wires"));
}

}  // namespace

LoadResult validateAndLoad(const QJsonObject &obj, Document &doc) {
	if (auto r = validate::schemaField(obj, QStringLiteral("document"), SchemaVersion); !r) {
		return r;
	}
	if (auto r = validate::object(obj["board"], QStringLiteral("board")); !r) {
		return r;
	}
	if (auto r = boardio::validateJson(obj["board"].toObject(), QStringLiteral("board")); !r) {
		return r;
	}
	if (obj.contains("placements")) {
		if (auto r = validate::array(obj["placements"], QStringLiteral("placements")); !r) return r;
		if (auto r = validatePlacementsArray(obj["placements"].toArray()); !r) return r;
	}
	if (obj.contains("wires")) {
		if (auto r = validate::array(obj["wires"], QStringLiteral("wires")); !r) return r;
		if (auto r = validateWiresArray(obj["wires"].toArray()); !r) return r;
	}
	if (obj.contains("dependencies")) {
		if (auto r = validate::array(obj["dependencies"], QStringLiteral("dependencies")); !r) return r;
		for (int i = 0; i < obj["dependencies"].toArray().size(); ++i) {
			if (auto r = validate::object(obj["dependencies"].toArray()[i],
										  QStringLiteral("dependencies[%1]").arg(i));
				!r) {
				return r;
			}
		}
	}

	fromJsonObject(obj, doc);
	return LoadResult::success();
}

bool save(const Document &doc, const QString &filePath) {
	QFile f(filePath);
	if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		return false;
	}
	f.write(QJsonDocument(toJsonObject(doc)).toJson(QJsonDocument::Indented));
	return true;
}

LoadResult load(const QString &filePath, Document &doc) {
	QFile f(filePath);
	if (!f.open(QIODevice::ReadOnly)) {
		return LoadResult::failure(QStringLiteral("ファイルを開けません"),
									QStringLiteral("ファイルを読み込めませんでした: %1").arg(filePath));
	}
	QJsonParseError err;
	const QJsonDocument jdoc = QJsonDocument::fromJson(f.readAll(), &err);
	if (err.error != QJsonParseError::NoError || !jdoc.isObject()) {
		return LoadResult::failure(QStringLiteral("ファイルが壊れています"),
									QStringLiteral("JSON として解析できません: %1").arg(err.errorString()));
	}
	return validateAndLoad(jdoc.object(), doc);
}

QSet<QString> requiredLibraryIds(const Document &doc) {
	// doc.dependencies はキャッシュ的な位置づけなので、実際に placements が参照している
	// ライブラリ id も必ず合わせて対象にする (dependencies の更新漏れがあっても、
	// エクスポートで参照先ライブラリが欠落しないようにするため)。
	QSet<QString> libraryIds;
	for (const auto &dep : doc.dependencies) {
		libraryIds.insert(dep.libraryId);
	}
	for (const auto &p : doc.placements) {
		libraryIds.insert(p->libraryId);
	}
	return libraryIds;
}

PackageResult exportPackage(const Document &doc, const LibraryResolver &resolver,
							const QSet<QString> &includeLibraryIds, const QString &bpkgFilePath) {
	PackageResult result;

	ZipWriter writer;
	if (!writer.addFile(QStringLiteral("document.boardes"), QJsonDocument(toJsonObject(doc)).toJson(QJsonDocument::Indented))) {
		result.error = writer.errorString();
		return result;
	}

	const QSet<QString> libraryIds = requiredLibraryIds(doc);

	for (const auto &libraryId : std::as_const(libraryIds)) {
		if (!includeLibraryIds.contains(libraryId)) {
			// ユーザーが同梱を選ばなかった (再配布不可の警告を見て見送った、等)。
			result.skippedLibraryIds.append(libraryId);
			continue;
		}
		const auto lib = resolver ? resolver(libraryId) : nullptr;
		if (!lib) {
			result.skippedLibraryIds.append(libraryId);
			continue;
		}
		const QString prefix = QStringLiteral("libraries/%1/").arg(lib->id);
		libraryio::PackageSink sink;
		sink.write = [&writer, &prefix](const QString &relPath, const QByteArray &data) -> bool {
			return writer.addFile(prefix + relPath, data);
		};
		if (!libraryio::writePackage(*lib, sink)) {
			result.error = QStringLiteral("ライブラリ %1 の書き出しに失敗しました").arg(lib->id);
			return result;
		}
		result.bundledLibraryIds.append(lib->id);
	}

	QJsonObject manifest;
	manifest["schema"] = QStringLiteral("boardes.package/1");
	manifest["bundledLibraries"] = QJsonArray::fromStringList(result.bundledLibraryIds);
	manifest["skippedLibraries"] = QJsonArray::fromStringList(result.skippedLibraryIds);
	if (!writer.addFile(QStringLiteral("MANIFEST.json"), QJsonDocument(manifest).toJson(QJsonDocument::Indented))) {
		result.error = writer.errorString();
		return result;
	}

	const QByteArray zipBytes = writer.finish();
	if (zipBytes.isEmpty() || !writer.isValid()) {
		result.error = writer.errorString();
		return result;
	}
	QFile f(bpkgFilePath);
	if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate) || f.write(zipBytes) != zipBytes.size()) {
		result.error = QStringLiteral("ファイルの書き込みに失敗しました: %1").arg(bpkgFilePath);
		return result;
	}

	result.ok = true;
	return result;
}

ImportResult importPackage(const QString &bpkgFilePath, Document &doc, const LibraryImporter &libraryImporter) {
	ImportResult result;
	ZipReader reader(bpkgFilePath);
	if (!reader.isValid()) {
		result.error = reader.errorString();
		return result;
	}
	// zip slip 対策: ".." や絶対パスを含むエントリ名があれば拒否する。
	for (const QString &name : reader.fileNames()) {
		if (const auto r = validate::safeZipEntryName(name); !r) {
			result.error = r.detail;
			return result;
		}
	}
	if (!reader.contains(QStringLiteral("document.boardes"))) {
		result.error = QStringLiteral("document.boardes が見つかりません");
		return result;
	}
	QJsonParseError err;
	const QJsonDocument jdoc = QJsonDocument::fromJson(reader.read(QStringLiteral("document.boardes")), &err);
	if (err.error != QJsonParseError::NoError || !jdoc.isObject()) {
		result.error = QStringLiteral("document.boardes の解析に失敗しました");
		return result;
	}
	const LoadResult docResult = validateAndLoad(jdoc.object(), doc);
	if (!docResult.ok) {
		result.error = QStringLiteral("%1\n%2").arg(docResult.summary, docResult.detail);
		return result;
	}

	// 同梱されたライブラリを探して呼び出し側に渡す。
	QSet<QString> libIds;
	for (const auto &name : reader.fileNames()) {
		if (!name.startsWith(QStringLiteral("libraries/"))) {
			continue;
		}
		const QStringList parts = name.split(QLatin1Char('/'));
		if (parts.size() >= 2) {
			libIds.insert(parts[1]);
		}
	}
	for (const auto &libId : std::as_const(libIds)) {
		const QString prefix = QStringLiteral("libraries/%1/").arg(libId);
		libraryio::PackageSource source;
		source.exists = [&reader, &prefix](const QString &relPath) -> bool {
			return reader.contains(prefix + relPath);
		};
		source.read = [&reader, &prefix](const QString &relPath) -> QByteArray {
			return reader.read(prefix + relPath);
		};
		const auto lib = libraryio::readPackage(source);
		if (lib && libraryImporter) {
			libraryImporter(libId, *lib);
		}
	}

	result.ok = true;
	return result;
}

}  // namespace documentio
