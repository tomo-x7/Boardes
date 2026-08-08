#include "libraryio.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <algorithm>

#include "boardio.h"
#include "jsonutil.h"
#include "partio.h"
#include "zipio.h"

namespace libraryio {

namespace {

// 廃止したライセンス種別のキー -> {表示名, 参照URL}。旧形式の library.json
// (cc-by-sa-4.0 等) を読んだとき、licenseKindFromKey() が Custom に落とした後で
// ここから customName/customUrl を補完する (Phase 14)。
struct LegacyLicenseHint {
	QString name;
	QString url;
};
const QHash<QString, LegacyLicenseHint> &legacyLicenseHints() {
	static const QHash<QString, LegacyLicenseHint> hints = {
		{QStringLiteral("cc-by-sa-4.0"),
		 {QStringLiteral("CC BY-SA 4.0"), QStringLiteral("https://creativecommons.org/licenses/by-sa/4.0/")}},
		{QStringLiteral("cc-by-nc-4.0"),
		 {QStringLiteral("CC BY-NC 4.0"), QStringLiteral("https://creativecommons.org/licenses/by-nc/4.0/")}},
		{QStringLiteral("apache-2.0"), {QStringLiteral("Apache License 2.0"), QStringLiteral("https://www.apache.org/licenses/LICENSE-2.0")}},
		{QStringLiteral("cern-ohl-p-2.0"),
		 {QStringLiteral("CERN-OHL-P-2.0"), QStringLiteral("https://ohwr.org/cern_ohl_p_v2.txt")}},
		{QStringLiteral("cern-ohl-s-2.0"),
		 {QStringLiteral("CERN-OHL-S-2.0"), QStringLiteral("https://ohwr.org/cern_ohl_s_v2.txt")}},
	};
	return hints;
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
	obj["redistribution"] = redistribution;

	// readOnly は Phase 14 で廃止 (全ライブラリ編集可)。過去のファイルとの互換のため
	// キー自体は false 固定で書いておく (読む側は無視する)。
	obj["readOnly"] = false;

	if (!lib.basedOn.isEmpty()) {
		QJsonArray basedArr;
		for (const auto &b : lib.basedOn) {
			QJsonObject based;
			based["id"] = b.libraryId;
			based["name"] = b.name;
			based["version"] = b.version;
			based["license"] = b.licenseLabel;
			basedArr.append(based);
		}
		obj["basedOn"] = basedArr;
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

namespace {
LoadResult fail(LoadResult *errorOut, const QString &summary, const QString &detail) {
	const LoadResult r = LoadResult::failure(summary, detail);
	if (errorOut) *errorOut = r;
	return r;
}
}  // namespace

std::optional<Library> readPackage(const PackageSource &source, LoadResult *errorOut) {
	if (!source.exists(QStringLiteral("library.json"))) {
		fail(errorOut, QStringLiteral("ライブラリが壊れています"), QStringLiteral("library.json が見つかりません"));
		return std::nullopt;
	}
	const QByteArray manifestBytes = source.read(QStringLiteral("library.json"));
	QJsonParseError err;
	const QJsonDocument doc = QJsonDocument::fromJson(manifestBytes, &err);
	if (err.error != QJsonParseError::NoError || !doc.isObject()) {
		fail(errorOut, QStringLiteral("ライブラリが壊れています"),
			 QStringLiteral("library.json を JSON として解析できません: %1").arg(err.errorString()));
		return std::nullopt;
	}
	const QJsonObject obj = doc.object();

	if (auto r = validate::schemaField(obj, QStringLiteral("library"), SchemaVersion); !r) {
		if (errorOut) *errorOut = r;
		return std::nullopt;
	}
	if (auto r = validate::nonEmptyString(obj["id"], QStringLiteral("id")); !r) {
		if (errorOut) *errorOut = r;
		return std::nullopt;
	}
	if (auto r = validate::nonEmptyString(obj["name"], QStringLiteral("name")); !r) {
		if (errorOut) *errorOut = r;
		return std::nullopt;
	}
	if (obj.contains("categories")) {
		if (auto r = validate::array(obj["categories"], QStringLiteral("categories")); !r) {
			if (errorOut) *errorOut = r;
			return std::nullopt;
		}
		QStringList catIds;
		for (const auto &v : obj["categories"].toArray()) {
			catIds << v.toObject()["id"].toString();
		}
		if (auto r = validate::uniqueIds(catIds, QStringLiteral("categories")); !r) {
			if (errorOut) *errorOut = r;
			return std::nullopt;
		}
	}

	Library lib;
	lib.id = obj["id"].toString();
	lib.name = obj["name"].toString();
	lib.version = obj["version"].toString();
	lib.author = obj["author"].toString();
	lib.authorUrl = obj["authorUrl"].toString();
	lib.homepage = obj["homepage"].toString();
	lib.description = obj["description"].toString();

	const QJsonObject license = obj["license"].toObject();
	const QString licenseKey = license["kind"].toString();
	lib.license.kind = licenseKindFromKey(licenseKey);
	lib.license.customName = license["customName"].toString();
	lib.license.customUrl = license["customUrl"].toString();
	// 廃止した種別のキーは licenseKindFromKey() が Custom に落とすが、customName が
	// 空のままだと単に「カスタム」としか表示できない。既知の旧キーなら表示名/URL を
	// 補完しておく (Phase 14)。
	if (lib.license.kind == LicenseKind::Custom && lib.license.customName.isEmpty()) {
		const auto hint = legacyLicenseHints().constFind(licenseKey);
		if (hint != legacyLicenseHints().constEnd()) {
			lib.license.customName = hint->name;
			if (lib.license.customUrl.isEmpty()) {
				lib.license.customUrl = hint->url;
			}
		}
	}
	if (license["hasLicenseFile"].toBool() && source.exists(QStringLiteral("LICENSE.txt"))) {
		lib.license.customLicenseText = QString::fromUtf8(source.read(QStringLiteral("LICENSE.txt")));
	}

	const QJsonObject redistribution = obj["redistribution"].toObject();
	lib.redistribution.allowed = redistribution["allowed"].toBool();
	lib.redistribution.attributionRequired = redistribution["attributionRequired"].toBool();

	// readOnly は Phase 14 で廃止。旧ファイルにキーが残っていても無視する
	// (全ライブラリ編集可という現行方針を優先する)。

	if (obj.contains("basedOn")) {
		const QJsonValue basedVal = obj["basedOn"];
		const QJsonArray basedArr = basedVal.isArray() ? basedVal.toArray() : QJsonArray{basedVal.toObject()};
		for (const auto &v : basedArr) {
			const QJsonObject based = v.toObject();
			if (based.isEmpty()) continue;
			BasedOn b;
			b.libraryId = based["id"].toString();
			b.name = based["name"].toString();
			b.version = based["version"].toString();
			b.licenseLabel = based["license"].toString();
			lib.basedOn.append(b);
		}
	}

	QStringList warnings;

	for (const auto &v : obj["categories"].toArray()) {
		const QJsonObject c = v.toObject();
		CategoryInfo cat;
		cat.id = c["id"].toString();
		cat.name = c["name"].toString();
		cat.order = c["order"].toInt();
		if (c.contains("icon")) {
			const QByteArray iconBytes = source.read(c["icon"].toString());
			// アイコンのデコード失敗は致命的ではない (アイコン無し表示にする)。
			if (iconBytes.isEmpty() || !cat.icon.loadFromData(iconBytes)) {
				warnings << QStringLiteral("カテゴリ %1 のアイコンを読み込めませんでした").arg(cat.id);
				cat.icon = QImage();
			}
		}
		lib.categories.append(cat);
	}

	partio::ArtworkResolver resolver = [&source](const QString &relPathFromPart) -> QByteArray {
		// resolver は part.json からの相対パス (ファイル名のみ) で呼ばれるので、
		// parts/ ディレクトリを補って解決する。
		return source.read(QStringLiteral("parts/") + relPathFromPart);
	};

	// 参照先の .bpart/.bboard を1件でも読めなければ、ライブラリ全体を読み込み失敗にする
	// (どの部品/基板が壊れているかを detail に書く)。
	for (const auto &v : obj["parts"].toArray()) {
		const QJsonObject p = v.toObject();
		const QString file = p["file"].toString();
		if (file.isEmpty() || !source.exists(file)) {
			fail(errorOut, QStringLiteral("ライブラリが壊れています"),
				 QStringLiteral("部品ファイルが見つかりません: %1").arg(file));
			return std::nullopt;
		}
		QJsonParseError perr;
		const QJsonDocument partDoc = QJsonDocument::fromJson(source.read(file), &perr);
		if (perr.error != QJsonParseError::NoError || !partDoc.isObject()) {
			fail(errorOut, QStringLiteral("ライブラリが壊れています"),
				 QStringLiteral("%1 を JSON として解析できません: %2").arg(file, perr.errorString()));
			return std::nullopt;
		}
		if (const auto r = partio::validateJson(partDoc.object(), file); !r) {
			if (errorOut) *errorOut = r;
			return std::nullopt;
		}
		Part part = partio::fromJsonObject(partDoc.object(), resolver);
		if (part.artwork.image.isNull()) {
			fail(errorOut, QStringLiteral("ライブラリが壊れています"),
				 QStringLiteral("%1 の画像をデコードできませんでした").arg(file));
			return std::nullopt;
		}
		const QString categoryId = p["category"].toString();
		// part.id を先に確定させておく。insert(part.id, make_shared<Part>(std::move(part))) の
		// ように同じ式の中で part.id を読みつつ part を move すると、関数引数の評価順序が
		// 未規定なため move が先に走って id が空文字列になることがある (実際に踏んだバグ)。
		const QString partId = part.id;
		if (!categoryId.isEmpty() &&
			std::none_of(lib.categories.begin(), lib.categories.end(),
						 [&](const CategoryInfo &c) { return c.id == categoryId; })) {
			// 未知のカテゴリを参照している: 致命的ではないので未分類に倒す。
			warnings << QStringLiteral("部品 %1 が存在しないカテゴリ %2 を参照しています (未分類にしました)")
								.arg(partId, categoryId);
			lib.partCategory.insert(partId, QString());
		} else {
			lib.partCategory.insert(partId, categoryId);
		}
		lib.parts.insert(partId, std::make_shared<Part>(std::move(part)));
	}

	for (const auto &v : obj["boards"].toArray()) {
		const QJsonObject b = v.toObject();
		const QString file = b["file"].toString();
		if (file.isEmpty() || !source.exists(file)) {
			fail(errorOut, QStringLiteral("ライブラリが壊れています"),
				 QStringLiteral("基板ファイルが見つかりません: %1").arg(file));
			return std::nullopt;
		}
		QJsonParseError berr;
		const QJsonDocument boardDoc = QJsonDocument::fromJson(source.read(file), &berr);
		if (berr.error != QJsonParseError::NoError || !boardDoc.isObject()) {
			fail(errorOut, QStringLiteral("ライブラリが壊れています"),
				 QStringLiteral("%1 を JSON として解析できません: %2").arg(file, berr.errorString()));
			return std::nullopt;
		}
		if (const auto r = boardio::validateJson(boardDoc.object(), file); !r) {
			if (errorOut) *errorOut = r;
			return std::nullopt;
		}
		auto board = std::make_shared<BoardSpec>(boardio::fromJsonObject(boardDoc.object()));
		lib.boards.insert(board->id, board);
	}

	if (errorOut) {
		errorOut->ok = true;
		errorOut->warnings = warnings;
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

std::optional<Library> loadFromDirectory(const QString &dirPath, LoadResult *errorOut) {
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
	return readPackage(source, errorOut);
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

std::optional<Library> importFromBlib(const QString &blibFilePath, LoadResult *errorOut) {
	ZipReader reader(blibFilePath);
	if (!reader.isValid()) {
		fail(errorOut, QStringLiteral("ファイルが壊れています"), reader.errorString());
		return std::nullopt;
	}
	// zip slip 対策: ".." や絶対パスを含むエントリ名があれば拒否する。
	for (const QString &name : reader.fileNames()) {
		if (const auto r = validate::safeZipEntryName(name); !r) {
			if (errorOut) *errorOut = r;
			return std::nullopt;
		}
	}
	PackageSource source;
	source.exists = [&reader](const QString &relPath) -> bool { return reader.contains(relPath); };
	source.read = [&reader](const QString &relPath) -> QByteArray { return reader.read(relPath); };
	return readPackage(source, errorOut);
}

}  // namespace libraryio
