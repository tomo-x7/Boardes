#include "librarymanager.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

#include "../io/boardio.h"
#include "../io/libraryio.h"
#include "../io/partio.h"
#include "../io/passimport.h"

namespace {
LibraryManager::OpResult errorResult(const QString &message) {
	LibraryManager::OpResult r;
	r.error = message;
	return r;
}
}  // namespace

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
	m_loadIssues.clear();

	QDir root(storageDir());
	if (root.exists()) {
		const QFileInfoList dirs = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
		for (const auto &fi : dirs) {
			if (fi.fileName().endsWith(QStringLiteral(".tmp"))) {
				// persist() が失敗した際の作業ディレクトリの残骸。無視する。
				continue;
			}
			LoadResult loadResult;
			auto lib = libraryio::loadFromDirectory(fi.absoluteFilePath(), &loadResult);
			if (lib && !lib->id.isEmpty()) {
				m_libraries.insert(lib->id, std::make_shared<Library>(*lib));
				for (const QString &w : loadResult.warnings) {
					qWarning("%s: %s", qUtf8Printable(fi.absoluteFilePath()), qUtf8Printable(w));
				}
			} else {
				const QString reason = loadResult.detail.isEmpty()
											? QStringLiteral("library.json を読み込めませんでした")
											: loadResult.detail;
				m_loadIssues.append({fi.absoluteFilePath(), reason});
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
		lib->redistribution.allowed = false;  // マイライブラリは既定でエクスポート不可 (バックアップ用途では可)
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
		lib->redistribution.allowed = false;  // 再配布不可 (手元でのバックアップ・編集は自由)
		m_libraries.insert(lib->id, lib);
		persist(*lib);
	}
}

bool LibraryManager::persist(const Library &lib) {
	// 一時ディレクトリに書き切ってから、成功した場合のみ本来のディレクトリと入れ替える。
	// 部品1件の追加のような小さな変更でも毎回ディレクトリ全体を書き直す都合上、
	// 途中で失敗する (ディスク満杯・権限エラー等) と旧実装では既存データごと消えていた。
	const QString dir = libraryDir(lib.id);
	const QString tmpDir = dir + QStringLiteral(".tmp");
	QDir(tmpDir).removeRecursively();
	if (!libraryio::saveToDirectory(lib, tmpDir)) {
		QDir(tmpDir).removeRecursively();
		return false;
	}
	QDir(dir).removeRecursively();
	if (!QDir().rename(tmpDir, dir)) {
		QDir(tmpDir).removeRecursively();
		return false;
	}
	return true;
}

std::shared_ptr<Part> LibraryManager::resolvePart(const QString &libraryId, const QString &partId) const {
	const auto lib = library(libraryId);
	return lib ? lib->part(partId) : nullptr;
}

std::shared_ptr<BoardSpec> LibraryManager::resolveBoard(const QString &libraryId, const QString &boardId) const {
	const auto lib = library(libraryId);
	return lib ? lib->board(boardId) : nullptr;
}

QString LibraryManager::uniquePartIdIn(const Library &lib, const QString &baseId) const {
	if (!lib.parts.contains(baseId)) {
		return baseId;
	}
	int n = 2;
	while (lib.parts.contains(baseId + QStringLiteral("-%1").arg(n))) {
		++n;
	}
	return baseId + QStringLiteral("-%1").arg(n);
}

QString LibraryManager::uniqueBoardIdIn(const Library &lib, const QString &baseId) const {
	if (!lib.boards.contains(baseId)) {
		return baseId;
	}
	int n = 2;
	while (lib.boards.contains(baseId + QStringLiteral("-%1").arg(n))) {
		++n;
	}
	return baseId + QStringLiteral("-%1").arg(n);
}

QString LibraryManager::uniquePartId(const QString &libId, const QString &baseId) const {
	const auto lib = library(libId);
	return lib ? uniquePartIdIn(*lib, baseId) : baseId;
}

QString LibraryManager::uniqueBoardId(const QString &libId, const QString &baseId) const {
	const auto lib = library(libId);
	return lib ? uniqueBoardIdIn(*lib, baseId) : baseId;
}

QString LibraryManager::uniquePartIdForMyLibrary(const QString &baseId) const {
	return uniquePartId(myLibraryId(), baseId);
}

QString LibraryManager::uniqueBoardIdForMyLibrary(const QString &baseId) const {
	return uniqueBoardId(myLibraryId(), baseId);
}

template <class F>
bool LibraryManager::mutateLibrary(const QString &libId, F mutator) {
	const auto existing = library(libId);
	if (!existing) {
		return false;
	}
	auto lib = std::make_shared<Library>(*existing);
	if (!mutator(*lib)) {
		return false;
	}
	if (!persist(*lib)) {
		return false;
	}
	m_libraries.insert(libId, lib);
	emit librariesChanged();
	return true;
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
	if (part->id.isEmpty()) {
		part->id = QFileInfo(filePath).completeBaseName();
	}
	return addPartTo(myLibraryId(), *part);
}

LibraryManager::OpResult LibraryManager::importBoardFile(const QString &filePath) {
	OpResult result;
	if (!filePath.toLower().endsWith(QStringLiteral(".bboard"))) {
		result.error = QStringLiteral("未対応の拡張子です (.bboard を指定してください)");
		return result;
	}
	LoadResult loadResult;
	const auto board = boardio::load(filePath, &loadResult);
	if (!board) {
		result.error = loadResult.detail.isEmpty()
							? QStringLiteral("基板ファイルを読み込めませんでした: %1").arg(filePath)
							: QStringLiteral("%1\n%2").arg(loadResult.summary, loadResult.detail);
		return result;
	}
	BoardSpec b = *board;
	if (b.id.isEmpty()) {
		b.id = QFileInfo(filePath).completeBaseName();
	}
	return addBoardTo(myLibraryId(), b);
}

LibraryManager::OpResult LibraryManager::createLibrary(const Library &meta) {
	if (meta.id.isEmpty()) {
		return errorResult(QStringLiteral("ライブラリ id が空です"));
	}
	if (m_libraries.contains(meta.id)) {
		return errorResult(QStringLiteral("id が既存のライブラリと重複しています: %1").arg(meta.id));
	}
	auto lib = std::make_shared<Library>(meta);
	if (lib->categories.isEmpty()) {
		CategoryInfo uncategorized;
		uncategorized.id = uncategorizedCategoryId();
		uncategorized.name = QStringLiteral("未分類");
		lib->categories.append(uncategorized);
	}
	if (!persist(*lib)) {
		return errorResult(QStringLiteral("ライブラリの保存に失敗しました"));
	}
	m_libraries.insert(lib->id, lib);

	OpResult result;
	result.ok = true;
	result.libraryId = lib->id;
	emit librariesChanged();
	return result;
}

bool LibraryManager::updateLibraryMetadata(const QString &id, const Library &updated) {
	const auto existing = library(id);
	if (!existing) {
		return false;
	}
	auto lib = std::make_shared<Library>(updated);
	lib->id = id;  // id は変えさせない

	if (!persist(*lib)) {
		return false;
	}
	m_libraries.insert(id, lib);
	emit librariesChanged();
	return true;
}

bool LibraryManager::removeLibrary(const QString &id) {
	if (!m_libraries.contains(id)) {
		return false;
	}
	QDir(libraryDir(id)).removeRecursively();
	m_libraries.remove(id);
	// マイライブラリ・PasS互換は概念上常に存在する前提のコードが他にあるため
	// (addPartToMyLibrary 等)、削除されたら空の状態で作り直す。
	ensureBuiltinLibraries();
	emit librariesChanged();
	return true;
}

bool LibraryManager::addCategory(const QString &libId, const CategoryInfo &cat) {
	return mutateLibrary(libId, [&](Library &lib) {
		if (cat.id.isEmpty() || lib.category(cat.id)) {
			return false;
		}
		lib.categories.append(cat);
		return true;
	});
}

bool LibraryManager::updateCategory(const QString &libId, const CategoryInfo &cat) {
	return mutateLibrary(libId, [&](Library &lib) {
		CategoryInfo *existing = lib.category(cat.id);
		if (!existing) {
			return false;
		}
		*existing = cat;
		return true;
	});
}

bool LibraryManager::removeCategory(const QString &libId, const QString &catId) {
	if (catId == uncategorizedCategoryId()) {
		return false;  // 未分類は削除できない (受け皿として常に必要)
	}
	return mutateLibrary(libId, [&](Library &lib) {
		int idx = -1;
		for (int i = 0; i < lib.categories.size(); ++i) {
			if (lib.categories[i].id == catId) {
				idx = i;
				break;
			}
		}
		if (idx < 0) {
			return false;
		}
		lib.categories.removeAt(idx);
		if (!lib.category(uncategorizedCategoryId())) {
			CategoryInfo uncategorized;
			uncategorized.id = uncategorizedCategoryId();
			uncategorized.name = QStringLiteral("未分類");
			lib.categories.append(uncategorized);
		}
		for (auto it = lib.partCategory.begin(); it != lib.partCategory.end(); ++it) {
			if (it.value() == catId) {
				it.value() = uncategorizedCategoryId();
			}
		}
		return true;
	});
}

bool LibraryManager::reorderCategories(const QString &libId, const QStringList &orderedIds) {
	return mutateLibrary(libId, [&](Library &lib) {
		for (int i = 0; i < orderedIds.size(); ++i) {
			if (auto *c = lib.category(orderedIds[i])) {
				c->order = i;
			}
		}
		return true;
	});
}

LibraryManager::OpResult LibraryManager::addPartTo(const QString &libId, const Part &part,
													const QString &categoryId) {
	if (part.id.isEmpty()) {
		return errorResult(QStringLiteral("部品 id が空です"));
	}
	const auto existing = library(libId);
	if (!existing) {
		return errorResult(QStringLiteral("ライブラリが見つかりません: %1").arg(libId));
	}
	auto lib = std::make_shared<Library>(*existing);
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
		return errorResult(QStringLiteral("ライブラリの保存に失敗しました"));
	}
	m_libraries.insert(lib->id, lib);

	OpResult result;
	result.ok = true;
	result.libraryId = lib->id;
	result.partCount = 1;
	emit librariesChanged();
	return result;
}

bool LibraryManager::removePartFrom(const QString &libId, const QString &partId) {
	return mutateLibrary(libId, [&](Library &lib) {
		if (!lib.parts.contains(partId)) {
			return false;
		}
		lib.parts.remove(partId);
		lib.partCategory.remove(partId);
		return true;
	});
}

bool LibraryManager::setPartCategory(const QString &libId, const QString &partId, const QString &categoryId) {
	return mutateLibrary(libId, [&](Library &lib) {
		if (!lib.parts.contains(partId) || !lib.category(categoryId)) {
			return false;
		}
		lib.partCategory.insert(partId, categoryId);
		return true;
	});
}

LibraryManager::OpResult LibraryManager::addBoardTo(const QString &libId, const BoardSpec &board) {
	if (board.id.isEmpty()) {
		return errorResult(QStringLiteral("基板 id が空です"));
	}
	const auto existing = library(libId);
	if (!existing) {
		return errorResult(QStringLiteral("ライブラリが見つかりません: %1").arg(libId));
	}
	auto lib = std::make_shared<Library>(*existing);
	lib->boards.insert(board.id, std::make_shared<BoardSpec>(board));

	if (!persist(*lib)) {
		return errorResult(QStringLiteral("ライブラリの保存に失敗しました"));
	}
	m_libraries.insert(lib->id, lib);

	OpResult result;
	result.ok = true;
	result.libraryId = lib->id;
	result.boardCount = 1;
	emit librariesChanged();
	return result;
}

bool LibraryManager::removeBoardFrom(const QString &libId, const QString &boardId) {
	return mutateLibrary(libId, [&](Library &lib) {
		if (!lib.boards.contains(boardId)) {
			return false;
		}
		lib.boards.remove(boardId);
		return true;
	});
}

LibraryManager::OpResult LibraryManager::addPartToMyLibrary(const Part &part, const QString &categoryId) {
	return addPartTo(myLibraryId(), part, categoryId);
}

LibraryManager::OpResult LibraryManager::addBoardToMyLibrary(const BoardSpec &board) {
	return addBoardTo(myLibraryId(), board);
}

LibraryManager::OpResult LibraryManager::copyPartsBetween(const QString &srcLibId, const QStringList &partIds,
														   const QString &dstLibId, const QString &dstCategoryId) {
	const auto src = library(srcLibId);
	const auto dstExisting = library(dstLibId);
	if (!src || !dstExisting) {
		return errorResult(QStringLiteral("ライブラリが見つかりません"));
	}
	auto dst = std::make_shared<Library>(*dstExisting);

	QString cat = dstCategoryId;
	if (cat.isEmpty() || !dst->category(cat)) {
		if (!dst->category(uncategorizedCategoryId())) {
			CategoryInfo uncategorized;
			uncategorized.id = uncategorizedCategoryId();
			uncategorized.name = QStringLiteral("未分類");
			dst->categories.append(uncategorized);
		}
		cat = uncategorizedCategoryId();
	}

	int copied = 0;
	for (const QString &partId : partIds) {
		const auto part = src->part(partId);
		if (!part) {
			continue;
		}
		Part copy = *part;
		copy.id = uniquePartIdIn(*dst, copy.id);
		dst->parts.insert(copy.id, std::make_shared<Part>(copy));
		dst->partCategory.insert(copy.id, cat);
		++copied;
	}
	if (copied == 0) {
		return errorResult(QStringLiteral("複製できる部品がありませんでした"));
	}

	if (!src->redistribution.allowed && dst->id != src->id) {
		bool already = false;
		for (const auto &b : dst->basedOn) {
			if (b.libraryId == src->id) {
				already = true;
				break;
			}
		}
		if (!already) {
			BasedOn based;
			based.libraryId = src->id;
			based.name = src->name;
			based.version = src->version;
			based.licenseLabel = src->license.displayName();
			dst->basedOn.append(based);
		}
	}

	if (!persist(*dst)) {
		return errorResult(QStringLiteral("ライブラリの保存に失敗しました"));
	}
	m_libraries.insert(dst->id, dst);

	OpResult result;
	result.ok = true;
	result.libraryId = dst->id;
	result.partCount = copied;
	emit librariesChanged();
	return result;
}

LibraryManager::OpResult LibraryManager::copyBoardsBetween(const QString &srcLibId, const QStringList &boardIds,
															const QString &dstLibId) {
	const auto src = library(srcLibId);
	const auto dstExisting = library(dstLibId);
	if (!src || !dstExisting) {
		return errorResult(QStringLiteral("ライブラリが見つかりません"));
	}
	auto dst = std::make_shared<Library>(*dstExisting);

	int copied = 0;
	for (const QString &boardId : boardIds) {
		const auto board = src->board(boardId);
		if (!board) {
			continue;
		}
		BoardSpec copy = *board;
		copy.id = uniqueBoardIdIn(*dst, copy.id);
		dst->boards.insert(copy.id, std::make_shared<BoardSpec>(copy));
		++copied;
	}
	if (copied == 0) {
		return errorResult(QStringLiteral("複製できる基板がありませんでした"));
	}

	if (!src->redistribution.allowed && dst->id != src->id) {
		bool already = false;
		for (const auto &b : dst->basedOn) {
			if (b.libraryId == src->id) {
				already = true;
				break;
			}
		}
		if (!already) {
			BasedOn based;
			based.libraryId = src->id;
			based.name = src->name;
			based.version = src->version;
			based.licenseLabel = src->license.displayName();
			dst->basedOn.append(based);
		}
	}

	if (!persist(*dst)) {
		return errorResult(QStringLiteral("ライブラリの保存に失敗しました"));
	}
	m_libraries.insert(dst->id, dst);

	OpResult result;
	result.ok = true;
	result.libraryId = dst->id;
	result.boardCount = copied;
	emit librariesChanged();
	return result;
}

LibraryManager::OpResult LibraryManager::installBlib(const QString &blibFilePath) {
	LoadResult loadResult;
	auto lib = libraryio::importFromBlib(blibFilePath, &loadResult);
	if (!lib) {
		return errorResult(loadResult.detail.isEmpty()
								? QStringLiteral(".blib を読み込めませんでした: %1").arg(blibFilePath)
								: QStringLiteral("%1\n%2").arg(loadResult.summary, loadResult.detail));
	}
	return installLibrary(*lib);
}

LibraryManager::OpResult LibraryManager::installLibrary(const Library &lib) {
	if (lib.id.isEmpty()) {
		return errorResult(QStringLiteral("ライブラリ id が空です"));
	}
	if (lib.id == myLibraryId() || lib.id == passCompatId()) {
		// 外部の .blib がこの id を騙ってビルトインライブラリを上書きすることを防ぐ。
		return errorResult(QStringLiteral("この id は予約されています: %1").arg(lib.id));
	}
	if (m_libraries.contains(lib.id)) {
		// 既にインストール済み。上書きはしない (壊れていない限りエラー扱いにはしない)。
		OpResult result;
		result.ok = true;
		result.libraryId = lib.id;
		const auto existing = m_libraries.value(lib.id);
		result.partCount = existing->parts.size();
		result.boardCount = existing->boards.size();
		result.categoryCount = existing->categories.size();
		return result;
	}

	auto libPtr = std::make_shared<Library>(lib);
	if (!persist(*libPtr)) {
		return errorResult(QStringLiteral("ライブラリの保存に失敗しました"));
	}
	m_libraries.insert(libPtr->id, libPtr);

	OpResult result;
	result.ok = true;
	result.libraryId = libPtr->id;
	result.partCount = libPtr->parts.size();
	result.boardCount = libPtr->boards.size();
	result.categoryCount = libPtr->categories.size();
	emit librariesChanged();
	return result;
}

LibraryManager::OpResult LibraryManager::duplicateLibrary(const QString &sourceLibraryId, const DuplicateSpec &spec) {
	const auto source = library(sourceLibraryId);
	if (!source) {
		return errorResult(QStringLiteral("複製元のライブラリが見つかりません: %1").arg(sourceLibraryId));
	}
	if (spec.newId.isEmpty()) {
		return errorResult(QStringLiteral("新しい id を指定してください"));
	}
	if (m_libraries.contains(spec.newId)) {
		return errorResult(QStringLiteral("id が既存のライブラリと重複しています: %1").arg(spec.newId));
	}

	auto lib = std::make_shared<Library>(*source);  // 部品・基板・カテゴリを丸ごとコピー
	lib->id = spec.newId;
	lib->name = spec.newName;
	lib->author = spec.newAuthor;
	lib->version = spec.newVersion;
	lib->license = spec.newLicense;
	lib->redistribution =
		spec.newLicense.kind == LicenseKind::Custom ? spec.newRedistribution : redistributionRuleFor(spec.newLicense.kind);

	BasedOn based;
	based.libraryId = source->id;
	based.name = source->name;
	based.version = source->version;
	based.licenseLabel = source->license.displayName();
	lib->basedOn.append(based);

	if (!persist(*lib)) {
		return errorResult(QStringLiteral("ライブラリの保存に失敗しました"));
	}
	m_libraries.insert(lib->id, lib);

	OpResult result;
	result.ok = true;
	result.libraryId = lib->id;
	result.partCount = lib->parts.size();
	result.boardCount = lib->boards.size();
	result.categoryCount = lib->categories.size();
	emit librariesChanged();
	return result;
}

bool LibraryManager::exportLibrary(const QString &id, const QString &blibFilePath) const {
	// Phase 14: 再配布不可でもバックアップ用途で書き出せる。再配布不可であることの
	// 警告は呼び出し側 (UI) の責務とする。
	const auto lib = library(id);
	if (!lib) {
		return false;
	}
	return libraryio::exportToBlib(*lib, blibFilePath);
}
