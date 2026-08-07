#include "librarymanager.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

#include "../io/libraryio.h"
#include "../io/partio.h"
#include "../io/boardio.h"
#include "../io/passimport.h"

LibraryManager::LibraryManager(QObject *parent) : QObject(parent) {
}

QString LibraryManager::storageDir() const {
	return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/libraries");
}

QString LibraryManager::libraryDir(const QString &id) const {
	return storageDir() + QLatin1Char('/') + id;
}

void LibraryManager::loadAll() {
	m_libraries.clear();

	QDir root(storageDir());
	if (root.exists()) {
		const QFileInfoList dirs = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
		for (const auto &fi : dirs) {
			auto lib = libraryio::loadFromDirectory(fi.absoluteFilePath());
			if (lib && !lib->id.isEmpty()) {
				m_libraries.insert(lib->id, std::make_shared<Library>(*lib));
			}
		}
	}

	ensureBuiltinLibraries();
	emit librariesChanged();
}

void LibraryManager::ensureBuiltinLibraries() {
	if (!m_libraries.contains(myLibraryId())) {
		auto lib = std::make_shared<Library>();
		lib->id = myLibraryId();
		lib->name = QStringLiteral("マイライブラリ");
		lib->author.clear();
		lib->version = QStringLiteral("1.0.0");
		lib->license.kind = LicenseKind::AllRightsReserved;
		lib->redistribution.allowed = false;  // マイライブラリは常にエクスポート不可
		lib->readOnly = false;
		CategoryInfo uncategorized;
		uncategorized.id = uncategorizedCategoryId();
		uncategorized.name = QStringLiteral("未分類");
		lib->categories.append(uncategorized);
		m_libraries.insert(lib->id, lib);
		persist(*lib);
	}

	if (!m_libraries.contains(passCompatId())) {
		auto lib = std::make_shared<Library>();
		lib->id = passCompatId();
		lib->name = QStringLiteral("PasS互換");
		lib->author = QStringLiteral("uaubn");
		lib->homepage = QStringLiteral("http://uaubn.g2.xrea.com/pass/");
		lib->description = QStringLiteral("PasS のパーツ・基板データを変換したライブラリ");
		lib->version = QStringLiteral("0");
		lib->license.kind = LicenseKind::Custom;
		lib->license.customName = QStringLiteral("PasS (学校・個人での使用に限り自由に使用可)");
		lib->redistribution.allowed = false;  // 再配布不可
		lib->readOnly = true;
		m_libraries.insert(lib->id, lib);
		persist(*lib);
	}
}

bool LibraryManager::persist(const Library &lib) {
	const QString dir = libraryDir(lib.id);
	QDir(dir).removeRecursively();  // 前回の内容を消してから書き直す (古い部品の消し忘れを防ぐ)
	return libraryio::saveToDirectory(lib, dir);
}

std::shared_ptr<Part> LibraryManager::resolvePart(const QString &libraryId, const QString &partId) const {
	const auto lib = library(libraryId);
	return lib ? lib->part(partId) : nullptr;
}

std::shared_ptr<BoardSpec> LibraryManager::resolveBoard(const QString &libraryId, const QString &boardId) const {
	const auto lib = library(libraryId);
	return lib ? lib->board(boardId) : nullptr;
}

QString LibraryManager::uniquePartId(const Library &lib, const QString &baseId) const {
	if (!lib.parts.contains(baseId)) {
		return baseId;
	}
	int n = 2;
	while (lib.parts.contains(baseId + QStringLiteral("-%1").arg(n))) {
		++n;
	}
	return baseId + QStringLiteral("-%1").arg(n);
}

QString LibraryManager::uniqueBoardId(const Library &lib, const QString &baseId) const {
	if (!lib.boards.contains(baseId)) {
		return baseId;
	}
	int n = 2;
	while (lib.boards.contains(baseId + QStringLiteral("-%1").arg(n))) {
		++n;
	}
	return baseId + QStringLiteral("-%1").arg(n);
}

QString LibraryManager::uniquePartIdForMyLibrary(const QString &baseId) const {
	const auto lib = library(myLibraryId());
	return lib ? uniquePartId(*lib, baseId) : baseId;
}

QString LibraryManager::uniqueBoardIdForMyLibrary(const QString &baseId) const {
	const auto lib = library(myLibraryId());
	return lib ? uniqueBoardId(*lib, baseId) : baseId;
}

LibraryManager::OpResult LibraryManager::importPassFolder(const QString &sourceDir) {
	OpResult result;
	auto lib = std::make_shared<Library>();
	lib->id = passCompatId();
	lib->name = QStringLiteral("PasS互換");
	lib->author = QStringLiteral("uaubn");
	lib->homepage = QStringLiteral("http://uaubn.g2.xrea.com/pass/");
	lib->description = QStringLiteral("PasS のパーツ・基板データを変換したライブラリ");
	lib->version = QStringLiteral("0");
	lib->license.kind = LicenseKind::Custom;
	lib->license.customName = QStringLiteral("PasS (学校・個人での使用に限り自由に使用可)");
	lib->redistribution.allowed = false;
	lib->readOnly = true;

	const auto importResult = passimport::importFromDirectory(sourceDir, *lib);
	if (!importResult.ok) {
		result.error = importResult.error;
		return result;
	}
	for (const auto &issue : importResult.issues) {
		result.issues.append(QStringLiteral("%1: %2").arg(issue.file, issue.reason));
	}

	if (!persist(*lib)) {
		result.error = QStringLiteral("ライブラリの保存に失敗しました");
		return result;
	}
	m_libraries.insert(lib->id, lib);

	result.ok = true;
	result.libraryId = lib->id;
	result.partCount = importResult.partCount;
	result.boardCount = importResult.boardCount;
	result.categoryCount = importResult.categoryCount;
	emit librariesChanged();
	return result;
}

LibraryManager::OpResult LibraryManager::importPartFile(const QString &filePath) {
	OpResult result;
	std::optional<Part> part;
	const QString lower = filePath.toLower();
	if (lower.endsWith(QStringLiteral(".bpart"))) {
		part = partio::loadEmbedded(filePath);
	} else if (lower.endsWith(QStringLiteral(".part.json"))) {
		part = partio::loadSidecar(filePath);
	} else {
		result.error = QStringLiteral("未対応の拡張子です (.bpart または .part.json を指定してください)");
		return result;
	}
	if (!part) {
		result.error = QStringLiteral("部品ファイルを読み込めませんでした: %1").arg(filePath);
		return result;
	}

	auto lib = std::make_shared<Library>(*m_libraries.value(myLibraryId()));
	if (part->id.isEmpty()) {
		part->id = QFileInfo(filePath).completeBaseName();
	}
	part->id = uniquePartId(*lib, part->id);
	lib->parts.insert(part->id, std::make_shared<Part>(*part));
	if (!lib->category(uncategorizedCategoryId())) {
		CategoryInfo uncategorized;
		uncategorized.id = uncategorizedCategoryId();
		uncategorized.name = QStringLiteral("未分類");
		lib->categories.append(uncategorized);
	}
	lib->partCategory.insert(part->id, uncategorizedCategoryId());

	if (!persist(*lib)) {
		result.error = QStringLiteral("マイライブラリの保存に失敗しました");
		return result;
	}
	m_libraries.insert(lib->id, lib);

	result.ok = true;
	result.libraryId = lib->id;
	result.partCount = 1;
	emit librariesChanged();
	return result;
}

LibraryManager::OpResult LibraryManager::importBoardFile(const QString &filePath) {
	OpResult result;
	if (!filePath.toLower().endsWith(QStringLiteral(".bboard"))) {
		result.error = QStringLiteral("未対応の拡張子です (.bboard を指定してください)");
		return result;
	}
	const auto board = boardio::load(filePath);
	if (!board) {
		result.error = QStringLiteral("基板ファイルを読み込めませんでした: %1").arg(filePath);
		return result;
	}

	auto lib = std::make_shared<Library>(*m_libraries.value(myLibraryId()));
	BoardSpec b = *board;
	if (b.id.isEmpty()) {
		b.id = QFileInfo(filePath).completeBaseName();
	}
	b.id = uniqueBoardId(*lib, b.id);
	lib->boards.insert(b.id, std::make_shared<BoardSpec>(b));

	if (!persist(*lib)) {
		result.error = QStringLiteral("マイライブラリの保存に失敗しました");
		return result;
	}
	m_libraries.insert(lib->id, lib);

	result.ok = true;
	result.libraryId = lib->id;
	result.boardCount = 1;
	emit librariesChanged();
	return result;
}

LibraryManager::OpResult LibraryManager::addPartToMyLibrary(const Part &part, const QString &categoryId) {
	OpResult result;
	if (part.id.isEmpty()) {
		result.error = QStringLiteral("部品 id が空です");
		return result;
	}

	auto lib = std::make_shared<Library>(*m_libraries.value(myLibraryId()));
	lib->parts.insert(part.id, std::make_shared<Part>(part));

	QString cat = categoryId;
	if (cat.isEmpty() || !lib->category(cat)) {
		if (!lib->category(uncategorizedCategoryId())) {
			CategoryInfo uncategorized;
			uncategorized.id = uncategorizedCategoryId();
			uncategorized.name = QStringLiteral("未分類");
			lib->categories.append(uncategorized);
		}
		cat = uncategorizedCategoryId();
	}
	lib->partCategory.insert(part.id, cat);

	if (!persist(*lib)) {
		result.error = QStringLiteral("マイライブラリの保存に失敗しました");
		return result;
	}
	m_libraries.insert(lib->id, lib);

	result.ok = true;
	result.libraryId = lib->id;
	result.partCount = 1;
	emit librariesChanged();
	return result;
}

LibraryManager::OpResult LibraryManager::addBoardToMyLibrary(const BoardSpec &board) {
	OpResult result;
	if (board.id.isEmpty()) {
		result.error = QStringLiteral("基板 id が空です");
		return result;
	}

	auto lib = std::make_shared<Library>(*m_libraries.value(myLibraryId()));
	lib->boards.insert(board.id, std::make_shared<BoardSpec>(board));

	if (!persist(*lib)) {
		result.error = QStringLiteral("マイライブラリの保存に失敗しました");
		return result;
	}
	m_libraries.insert(lib->id, lib);

	result.ok = true;
	result.libraryId = lib->id;
	result.boardCount = 1;
	emit librariesChanged();
	return result;
}

LibraryManager::OpResult LibraryManager::installBlib(const QString &blibFilePath) {
	OpResult result;
	auto lib = libraryio::importFromBlib(blibFilePath);
	if (!lib) {
		result.error = QStringLiteral(".blib を読み込めませんでした: %1").arg(blibFilePath);
		return result;
	}
	return installLibrary(*lib);
}

LibraryManager::OpResult LibraryManager::installLibrary(const Library &lib) {
	OpResult result;
	if (lib.id.isEmpty()) {
		result.error = QStringLiteral("ライブラリ id が空です");
		return result;
	}
	if (lib.id == myLibraryId() || lib.id == passCompatId()) {
		result.error = QStringLiteral("この id は予約されています: %1").arg(lib.id);
		return result;
	}
	if (m_libraries.contains(lib.id)) {
		// 既にインストール済み。上書きはしない (壊れていない限りエラー扱いにはしない)。
		result.ok = true;
		result.libraryId = lib.id;
		const auto existing = m_libraries.value(lib.id);
		result.partCount = existing->parts.size();
		result.boardCount = existing->boards.size();
		result.categoryCount = existing->categories.size();
		return result;
	}

	auto libPtr = std::make_shared<Library>(lib);
	libPtr->readOnly = true;  // インストールしたライブラリは編集不可 (複製すれば編集可能)
	if (!persist(*libPtr)) {
		result.error = QStringLiteral("ライブラリの保存に失敗しました");
		return result;
	}
	m_libraries.insert(libPtr->id, libPtr);

	result.ok = true;
	result.libraryId = libPtr->id;
	result.partCount = libPtr->parts.size();
	result.boardCount = libPtr->boards.size();
	result.categoryCount = libPtr->categories.size();
	emit librariesChanged();
	return result;
}

LibraryManager::OpResult LibraryManager::duplicateLibrary(const QString &sourceLibraryId, const DuplicateSpec &spec) {
	OpResult result;
	const auto source = library(sourceLibraryId);
	if (!source) {
		result.error = QStringLiteral("複製元のライブラリが見つかりません: %1").arg(sourceLibraryId);
		return result;
	}
	if (spec.newId.isEmpty()) {
		result.error = QStringLiteral("新しい id を指定してください");
		return result;
	}
	if (m_libraries.contains(spec.newId)) {
		result.error = QStringLiteral("id が既存のライブラリと重複しています: %1").arg(spec.newId);
		return result;
	}

	auto lib = std::make_shared<Library>(*source);  // 部品・基板・カテゴリを丸ごとコピー
	lib->id = spec.newId;
	lib->name = spec.newName;
	lib->author = spec.newAuthor;
	lib->version = spec.newVersion;
	lib->license = spec.newLicense;
	lib->redistribution = redistributionRuleFor(spec.newLicense.kind);
	lib->readOnly = false;

	BasedOn based;
	based.libraryId = source->id;
	based.name = source->name;
	based.version = source->version;
	based.licenseLabel = source->license.displayName();
	lib->basedOn = based;

	if (!persist(*lib)) {
		result.error = QStringLiteral("ライブラリの保存に失敗しました");
		return result;
	}
	m_libraries.insert(lib->id, lib);

	result.ok = true;
	result.libraryId = lib->id;
	result.partCount = lib->parts.size();
	result.boardCount = lib->boards.size();
	result.categoryCount = lib->categories.size();
	emit librariesChanged();
	return result;
}

bool LibraryManager::updateLibraryMetadata(const QString &id, const Library &updated) {
	const auto existing = library(id);
	if (!existing || existing->readOnly) {
		return false;
	}
	auto lib = std::make_shared<Library>(updated);
	lib->id = id;                        // id は変えさせない
	lib->readOnly = existing->readOnly;  // 常に false のはずだが念のため踏襲する

	if (!persist(*lib)) {
		return false;
	}
	m_libraries.insert(id, lib);
	emit librariesChanged();
	return true;
}

bool LibraryManager::removeLibrary(const QString &id) {
	if (id == myLibraryId() || id == passCompatId()) {
		return false;
	}
	if (!m_libraries.contains(id)) {
		return false;
	}
	QDir(libraryDir(id)).removeRecursively();
	m_libraries.remove(id);
	emit librariesChanged();
	return true;
}

bool LibraryManager::exportLibrary(const QString &id, const QString &blibFilePath) const {
	const auto lib = library(id);
	if (!lib || !lib->redistribution.allowed || id == myLibraryId()) {
		return false;
	}
	return libraryio::exportToBlib(*lib, blibFilePath);
}
