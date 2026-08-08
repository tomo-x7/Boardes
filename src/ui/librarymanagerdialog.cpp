#include "librarymanagerdialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <algorithm>

#include "../model/librarymanager.h"
#include "boardeditordialog.h"
#include "duplicatelibrarydialog.h"
#include "helphint.h"
#include "librarymetadatadialog.h"
#include "parteditordialog.h"
#include "partpickerdialog.h"

namespace {
constexpr int kTypeRole = Qt::UserRole;
constexpr int kLibIdRole = Qt::UserRole + 1;
constexpr int kItemIdRole = Qt::UserRole + 2;

using NodeType = LibraryManagerDialog::NodeType;
}  // namespace

// ---------------------------------------------------------------- カテゴリ編集の小ダイアログ

namespace {
class CategoryEditDialog : public QDialog {
public:
	explicit CategoryEditDialog(QWidget *parent = nullptr) : QDialog(parent) {
		setWindowTitle(tr("カテゴリ"));
		m_idEdit = new QLineEdit(this);
		m_nameEdit = new QLineEdit(this);
		auto *form = new QFormLayout();
		form->addRow(tr("ID:"), m_idEdit);
		form->addRow(tr("表示名:"), m_nameEdit);
		auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
		connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
		connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
		auto *layout = new QVBoxLayout(this);
		layout->addLayout(form);
		layout->addWidget(buttons);
	}
	void setCategory(const CategoryInfo &cat, bool idEditable) {
		m_idEdit->setText(cat.id);
		m_idEdit->setEnabled(idEditable);
		m_nameEdit->setText(cat.name);
	}
	CategoryInfo category() const {
		CategoryInfo cat;
		cat.id = m_idEdit->text().trimmed();
		cat.name = m_nameEdit->text().trimmed();
		return cat;
	}

private:
	QLineEdit *m_idEdit;
	QLineEdit *m_nameEdit;
};
}  // namespace

// ---------------------------------------------------------------- LibraryManagerDialog

LibraryManagerDialog::LibraryManagerDialog(LibraryManager *libraryManager, QWidget *parent)
	: QDialog(parent), m_libraryManager(libraryManager) {
	setWindowTitle(tr("ライブラリ管理"));

	m_tree = new QTreeWidget(this);
	m_tree->setHeaderHidden(true);
	m_tree->setSelectionMode(QAbstractItemView::SingleSelection);

	m_detailsLabel = new QLabel(this);
	m_detailsLabel->setWordWrap(true);
	m_detailsLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
	m_detailsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

	auto *libraryGroup = new QGroupBox(tr("ライブラリ"), this);
	m_newLibraryButton = new QPushButton(tr("新規ライブラリ..."), this);
	m_duplicateButton = new QPushButton(tr("複製..."), this);
	m_removeButton = new QPushButton(tr("ライブラリを削除"), this);
	m_editMetadataButton = new QPushButton(tr("メタデータ編集..."), this);
	m_exportButton = new QPushButton(tr("エクスポート (.blib)..."), this);
	auto *libraryLayout = new QVBoxLayout(libraryGroup);
	libraryLayout->addWidget(m_newLibraryButton);
	libraryLayout->addWidget(m_duplicateButton);
	libraryLayout->addWidget(m_removeButton);
	libraryLayout->addWidget(m_editMetadataButton);
	libraryLayout->addWidget(m_exportButton);

	auto *importGroup = new QGroupBox(tr("取り込み"), this);
	auto *installButton = new QPushButton(tr(".blib をインストール..."), this);
	auto *importPassButton = new QPushButton(tr("PasS パーツフォルダ..."), this);
	auto *importPartButton = new QPushButton(tr("部品ファイル (.bpart/.part.json)..."), this);
	auto *importBoardButton = new QPushButton(tr("基板ファイル (.bboard)..."), this);
	auto *importLayout = new QVBoxLayout(importGroup);
	importLayout->addWidget(installButton);
	importLayout->addWidget(importPassButton);
	importLayout->addWidget(importPartButton);
	importLayout->addWidget(importBoardButton);

	auto *itemGroup = new QGroupBox(tr("選択中の項目"), this);
	m_addPartButton = new QPushButton(tr("部品を追加..."), this);
	m_addBoardButton = new QPushButton(tr("基板を追加..."), this);
	m_copyFromOtherButton = new QPushButton(tr("他ライブラリから複製..."), this);
	m_editSelectedButton = new QPushButton(tr("編集..."), this);
	m_deleteSelectedButton = new QPushButton(tr("選択項目を削除"), this);
	m_changeCategoryButton = new QPushButton(tr("カテゴリを変更..."), this);
	m_useBoardButton = new QPushButton(tr("現在のデザインに使用"), this);
	auto *itemLayout = new QVBoxLayout(itemGroup);
	itemLayout->addWidget(m_addPartButton);
	itemLayout->addWidget(m_addBoardButton);
	itemLayout->addWidget(m_copyFromOtherButton);
	itemLayout->addWidget(m_editSelectedButton);
	itemLayout->addWidget(m_deleteSelectedButton);
	itemLayout->addWidget(m_changeCategoryButton);
	itemLayout->addWidget(m_useBoardButton);

	auto *categoryGroup = new QGroupBox(tr("カテゴリ"), this);
	m_addCategoryButton = new QPushButton(tr("追加..."), this);
	m_renameCategoryButton = new QPushButton(tr("名前変更..."), this);
	m_deleteCategoryButton = new QPushButton(tr("カテゴリを削除"), this);
	auto *categoryLayout = new QHBoxLayout(categoryGroup);
	categoryLayout->addWidget(m_addCategoryButton);
	categoryLayout->addWidget(m_renameCategoryButton);
	categoryLayout->addWidget(m_deleteCategoryButton);

	auto *rightColumn = new QVBoxLayout();
	rightColumn->addWidget(m_detailsLabel);
	rightColumn->addWidget(libraryGroup);
	rightColumn->addWidget(importGroup);
	rightColumn->addWidget(itemGroup);
	rightColumn->addWidget(categoryGroup);
	rightColumn->addStretch(1);

	auto *columns = new QHBoxLayout();
	columns->addWidget(m_tree, /*stretch=*/1);
	columns->addLayout(rightColumn);

	auto *closeButtons = new QDialogButtonBox(QDialogButtonBox::Close, this);
	connect(closeButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	auto *mainLayout = new QVBoxLayout(this);
	mainLayout->addLayout(columns, /*stretch=*/1);
	mainLayout->addWidget(closeButtons);
	resize(760, 560);

	connect(m_tree, &QTreeWidget::currentItemChanged, this, &LibraryManagerDialog::onSelectionChanged);
	connect(m_newLibraryButton, &QPushButton::clicked, this, &LibraryManagerDialog::onNewLibrary);
	connect(m_duplicateButton, &QPushButton::clicked, this, &LibraryManagerDialog::onDuplicate);
	connect(m_removeButton, &QPushButton::clicked, this, &LibraryManagerDialog::onRemoveLibrary);
	connect(m_editMetadataButton, &QPushButton::clicked, this, &LibraryManagerDialog::onEditMetadata);
	connect(m_exportButton, &QPushButton::clicked, this, &LibraryManagerDialog::onExport);
	connect(installButton, &QPushButton::clicked, this, &LibraryManagerDialog::onInstallBlib);
	connect(importPassButton, &QPushButton::clicked, this, &LibraryManagerDialog::onImportPassFolder);
	connect(importPartButton, &QPushButton::clicked, this, &LibraryManagerDialog::onImportPartFile);
	connect(importBoardButton, &QPushButton::clicked, this, &LibraryManagerDialog::onImportBoardFile);
	connect(m_addPartButton, &QPushButton::clicked, this, &LibraryManagerDialog::onAddPart);
	connect(m_addBoardButton, &QPushButton::clicked, this, &LibraryManagerDialog::onAddBoard);
	connect(m_copyFromOtherButton, &QPushButton::clicked, this, &LibraryManagerDialog::onCopyFromOtherLibrary);
	connect(m_editSelectedButton, &QPushButton::clicked, this, &LibraryManagerDialog::onEditSelected);
	connect(m_deleteSelectedButton, &QPushButton::clicked, this, &LibraryManagerDialog::onDeleteSelectedItem);
	connect(m_changeCategoryButton, &QPushButton::clicked, this, &LibraryManagerDialog::onChangeCategory);
	connect(m_useBoardButton, &QPushButton::clicked, this, &LibraryManagerDialog::onUseSelectedBoard);
	connect(m_addCategoryButton, &QPushButton::clicked, this, &LibraryManagerDialog::onAddCategory);
	connect(m_renameCategoryButton, &QPushButton::clicked, this, &LibraryManagerDialog::onRenameCategory);
	connect(m_deleteCategoryButton, &QPushButton::clicked, this, &LibraryManagerDialog::onDeleteCategory);

	connect(m_libraryManager, &LibraryManager::librariesChanged, this, &LibraryManagerDialog::refreshTree);
	refreshTree();
}

namespace {
void setNodeData(QTreeWidgetItem *item, NodeType type, const QString &libId, const QString &itemId) {
	item->setData(0, kTypeRole, static_cast<int>(type));
	item->setData(0, kLibIdRole, libId);
	item->setData(0, kItemIdRole, itemId);
}
}  // namespace

void LibraryManagerDialog::refreshTree() {
	const Selection prev = currentSelection();

	m_tree->clear();
	const auto &libs = m_libraryManager->libraries();
	QTreeWidgetItem *toReselect = nullptr;

	for (auto it = libs.constBegin(); it != libs.constEnd(); ++it) {
		const auto &lib = it.value();
		auto *libItem = new QTreeWidgetItem(m_tree);
		libItem->setText(0, tr("%1 (%2)").arg(lib->name, lib->id));
		setNodeData(libItem, NodeType::Library, lib->id, QString());
		if (prev.type == NodeType::Library && prev.libId == lib->id) toReselect = libItem;

		auto *catRoot = new QTreeWidgetItem(libItem);
		catRoot->setText(0, tr("カテゴリ"));
		setNodeData(catRoot, NodeType::CategoryRoot, lib->id, QString());
		if (prev.type == NodeType::CategoryRoot && prev.libId == lib->id) toReselect = catRoot;

		QVector<CategoryInfo> cats = lib->categories;
		std::sort(cats.begin(), cats.end(), [](const CategoryInfo &a, const CategoryInfo &b) {
			return a.order < b.order;
		});
		for (const auto &cat : cats) {
			const auto partIds = lib->partIdsInCategory(cat.id);
			auto *catItem = new QTreeWidgetItem(catRoot);
			catItem->setText(0, tr("%1 (%2)").arg(cat.name).arg(partIds.size()));
			setNodeData(catItem, NodeType::Category, lib->id, cat.id);
			if (prev.type == NodeType::Category && prev.libId == lib->id && prev.itemId == cat.id) {
				toReselect = catItem;
			}

			QStringList sortedIds(partIds.begin(), partIds.end());
			std::sort(sortedIds.begin(), sortedIds.end(), [&](const QString &a, const QString &b) {
				const auto pa = lib->part(a);
				const auto pb = lib->part(b);
				return (pa ? pa->name : a) < (pb ? pb->name : b);
			});
			for (const auto &pid : sortedIds) {
				const auto part = lib->part(pid);
				auto *partItem = new QTreeWidgetItem(catItem);
				partItem->setText(0, part ? part->name : pid);
				setNodeData(partItem, NodeType::Part, lib->id, pid);
				if (prev.type == NodeType::Part && prev.libId == lib->id && prev.itemId == pid) toReselect = partItem;
			}
		}

		auto *boardRoot = new QTreeWidgetItem(libItem);
		boardRoot->setText(0, tr("基板 (%1)").arg(lib->boards.size()));
		setNodeData(boardRoot, NodeType::BoardRoot, lib->id, QString());
		if (prev.type == NodeType::BoardRoot && prev.libId == lib->id) toReselect = boardRoot;

		// lib->boards.keys() を2回呼んで begin()/end() をそれぞれ取ると、別々の一時
		// QList を指すイテレータの組み合わせになり範囲が壊れる (実際に踏んだバグ:
		// std::distance が無意味な値を返し bad_alloc で落ちた)。1回だけ呼んで束縛する
		// (この後 std::sort で並べ替えるので const にはしない)。
		QStringList boardIds = lib->boards.keys();
		std::sort(boardIds.begin(), boardIds.end(), [&](const QString &a, const QString &b) {
			const auto ba = lib->board(a);
			const auto bb = lib->board(b);
			return (ba ? ba->name : a) < (bb ? bb->name : b);
		});
		for (const auto &bid : boardIds) {
			const auto board = lib->board(bid);
			auto *boardItem = new QTreeWidgetItem(boardRoot);
			boardItem->setText(0, board ? board->name : bid);
			setNodeData(boardItem, NodeType::Board, lib->id, bid);
			if (prev.type == NodeType::Board && prev.libId == lib->id && prev.itemId == bid) toReselect = boardItem;
		}
	}

	m_tree->expandAll();
	if (toReselect) {
		m_tree->setCurrentItem(toReselect);
	} else if (m_tree->topLevelItemCount() > 0) {
		m_tree->setCurrentItem(m_tree->topLevelItem(0));
	}
	updateButtonStates();
	updateDetails();
}

LibraryManagerDialog::Selection LibraryManagerDialog::currentSelection() const {
	Selection sel;
	QTreeWidgetItem *item = m_tree->currentItem();
	if (!item) {
		return sel;
	}
	sel.type = static_cast<NodeType>(item->data(0, kTypeRole).toInt());
	sel.libId = item->data(0, kLibIdRole).toString();
	sel.itemId = item->data(0, kItemIdRole).toString();
	return sel;
}

QString LibraryManagerDialog::contextLibraryId() const {
	return currentSelection().libId;
}

QString LibraryManagerDialog::contextCategoryId() const {
	const Selection sel = currentSelection();
	return sel.type == NodeType::Category ? sel.itemId : QString();
}

void LibraryManagerDialog::onSelectionChanged() {
	updateButtonStates();
	updateDetails();
}

void LibraryManagerDialog::updateButtonStates() {
	const Selection sel = currentSelection();
	const bool hasLib = !sel.libId.isEmpty();

	m_duplicateButton->setEnabled(sel.type == NodeType::Library);
	m_editMetadataButton->setEnabled(sel.type == NodeType::Library);
	m_exportButton->setEnabled(sel.type == NodeType::Library);
	m_removeButton->setEnabled(sel.type == NodeType::Library);

	m_addPartButton->setEnabled(hasLib);
	m_addBoardButton->setEnabled(hasLib);
	m_copyFromOtherButton->setEnabled(hasLib);
	m_editSelectedButton->setEnabled(sel.type == NodeType::Part || sel.type == NodeType::Board);
	m_deleteSelectedButton->setEnabled(sel.type == NodeType::Part || sel.type == NodeType::Board);
	m_changeCategoryButton->setEnabled(sel.type == NodeType::Part);
	m_useBoardButton->setEnabled(sel.type == NodeType::Board);

	m_addCategoryButton->setEnabled(hasLib);
	m_renameCategoryButton->setEnabled(sel.type == NodeType::Category);
	m_deleteCategoryButton->setEnabled(sel.type == NodeType::Category &&
									   sel.itemId != LibraryManager::uncategorizedCategoryId());
}

void LibraryManagerDialog::updateDetails() {
	const Selection sel = currentSelection();
	const auto lib = sel.libId.isEmpty() ? nullptr : m_libraryManager->library(sel.libId);
	if (!lib) {
		m_detailsLabel->setText(tr("(選択されていません)"));
		return;
	}

	QStringList lines;
	if (sel.type == NodeType::Part) {
		const auto part = lib->part(sel.itemId);
		if (part) {
			lines << tr("部品: %1 (%2)").arg(part->name, part->id);
			lines << tr("ピン: %1個").arg(part->pins.size());
		}
	} else if (sel.type == NodeType::Board) {
		const auto board = lib->board(sel.itemId);
		if (board) {
			lines << tr("基板: %1 (%2)").arg(board->name, board->id);
			lines << tr("サイズ: %1 x %2 単位").arg(board->size.width()).arg(board->size.height());
		}
	} else {
		lines << tr("名前: %1").arg(lib->name);
		lines << tr("ID: %1").arg(lib->id);
		lines << tr("バージョン: %1").arg(lib->version.isEmpty() ? tr("(未設定)") : lib->version);
		lines << tr("作者: %1").arg(lib->author.isEmpty() ? tr("(未設定)") : lib->author);
		lines << tr("ライセンス: %1").arg(lib->license.displayName());
		lines << tr("再配布: %1").arg(lib->redistribution.allowed ? tr("可") : tr("不可"));
		lines << tr("部品: %1件 / 基板: %2件 / カテゴリ: %3件")
					 .arg(lib->parts.size())
					 .arg(lib->boards.size())
					 .arg(lib->categories.size());
		if (!lib->basedOn.isEmpty()) {
			QStringList based;
			for (const auto &b : lib->basedOn) {
				based << tr("%1 (v%2, %3)").arg(b.name, b.version, b.licenseLabel);
			}
			lines << tr("取り込み元: %1").arg(based.join(QStringLiteral(", ")));
		}
		if (!lib->description.isEmpty()) {
			lines << QString();
			lines << lib->description;
		}
	}
	m_detailsLabel->setText(lines.join(QStringLiteral("\n")));
}

bool LibraryManagerDialog::confirmRedistributionWarning(const QString &libId) {
	const auto lib = m_libraryManager->library(libId);
	if (!lib || lib->redistribution.allowed) {
		return true;
	}
	const auto btn = QMessageBox::question(
		this, tr("確認"),
		tr("「%1」は再配布が許可されていません (ライセンス: %2)。\n"
		   "書き出したファイルはバックアップや自分用の持ち出しのみに使い、"
		   "第三者への配布や公開はしないでください。続けますか？")
			.arg(lib->name, lib->license.displayName()));
	return btn == QMessageBox::Yes;
}

// ---------------------------------------------------------------- ライブラリ

void LibraryManagerDialog::onNewLibrary() {
	LibraryMetadataDialog dialog(this);
	if (dialog.exec() != QDialog::Accepted) {
		return;
	}
	Library meta;
	dialog.applyTo(meta);
	const auto result = m_libraryManager->createLibrary(meta);
	if (!result.ok) {
		QMessageBox::warning(this, tr("作成失敗"), result.error);
	}
}

void LibraryManagerDialog::onDuplicate() {
	const QString id = contextLibraryId();
	const auto lib = id.isEmpty() ? nullptr : m_libraryManager->library(id);
	if (!lib) {
		return;
	}
	DuplicateLibraryDialog dialog(*lib, this);
	if (dialog.exec() != QDialog::Accepted) {
		return;
	}
	const auto spec = dialog.spec();
	const auto result = m_libraryManager->duplicateLibrary(id, spec);
	if (!result.ok) {
		QMessageBox::warning(this, tr("複製失敗"), result.error);
		return;
	}
	QMessageBox::information(this, tr("複製完了"), tr("「%1」として複製しました。").arg(spec.newName));
}

void LibraryManagerDialog::onExport() {
	const QString id = contextLibraryId();
	if (id.isEmpty()) {
		return;
	}
	if (!confirmRedistributionWarning(id)) {
		return;
	}
	QString path =
		QFileDialog::getSaveFileName(this, tr("ライブラリをエクスポート"), QString(), tr("ライブラリパッケージ (*.blib)"));
	if (path.isEmpty()) {
		return;
	}
	if (!path.endsWith(QStringLiteral(".blib"), Qt::CaseInsensitive)) {
		path += QStringLiteral(".blib");
	}
	if (!m_libraryManager->exportLibrary(id, path)) {
		QMessageBox::warning(this, tr("エクスポート失敗"), tr("エクスポートできませんでした。"));
		return;
	}
	QMessageBox::information(this, tr("エクスポート完了"), tr("エクスポートしました: %1").arg(path));
}

void LibraryManagerDialog::onRemoveLibrary() {
	const QString id = contextLibraryId();
	const auto lib = id.isEmpty() ? nullptr : m_libraryManager->library(id);
	if (!lib) {
		return;
	}
	const auto btn = QMessageBox::question(
		this, tr("確認"),
		tr("ライブラリ「%1」を削除します。これを参照している設計データは、開いたときに部品が"
		   "解決できなくなります。よろしいですか？")
			.arg(lib->name));
	if (btn != QMessageBox::Yes) {
		return;
	}
	if (!m_libraryManager->removeLibrary(id)) {
		QMessageBox::warning(this, tr("削除失敗"), tr("削除できませんでした。"));
	}
}

void LibraryManagerDialog::onEditMetadata() {
	const QString id = contextLibraryId();
	const auto lib = id.isEmpty() ? nullptr : m_libraryManager->library(id);
	if (!lib) {
		return;
	}
	LibraryMetadataDialog dialog(this);
	dialog.setLibrary(*lib);
	if (dialog.exec() != QDialog::Accepted) {
		return;
	}
	Library updated = *lib;
	dialog.applyTo(updated);
	if (!m_libraryManager->updateLibraryMetadata(id, updated)) {
		QMessageBox::warning(this, tr("更新失敗"), tr("メタデータを更新できませんでした。"));
	}
}

// ---------------------------------------------------------------- 取り込み

void LibraryManagerDialog::onInstallBlib() {
	const QString path =
		QFileDialog::getOpenFileName(this, tr("ライブラリをインストール"), QString(), tr("ライブラリパッケージ (*.blib)"));
	if (path.isEmpty()) {
		return;
	}
	const auto result = m_libraryManager->installBlib(path);
	if (!result.ok) {
		QMessageBox::warning(this, tr("インストール失敗"), result.error);
		return;
	}
	QMessageBox::information(this, tr("インストール完了"),
							 tr("部品: %1件 / 基板: %2件 / カテゴリ: %3件をインストールしました。")
								 .arg(result.partCount)
								 .arg(result.boardCount)
								 .arg(result.categoryCount));
}

void LibraryManagerDialog::onImportPassFolder() {
	const QString dir = QFileDialog::getExistingDirectory(this, tr("PasS の parts フォルダを選択"));
	if (dir.isEmpty()) {
		return;
	}
	const auto existing = m_libraryManager->library(LibraryManager::passCompatId());
	if (existing && !existing->parts.isEmpty()) {
		const auto btn = QMessageBox::question(this, tr("確認"), tr("既存の「PasS互換」ライブラリを置き換えます。よろしいですか？"));
		if (btn != QMessageBox::Yes) {
			return;
		}
	}
	const auto result = m_libraryManager->importPassFolder(dir);
	if (!result.ok) {
		QMessageBox::warning(this, tr("インポート失敗"), result.error);
		return;
	}
	QString message =
		tr("カテゴリ: %1件 / 部品: %2件 / 基板: %3件を取り込みました。")
			.arg(result.categoryCount)
			.arg(result.partCount)
			.arg(result.boardCount);
	if (!result.issues.isEmpty()) {
		message += tr("\n\n読み込めなかったファイル (%1件):\n").arg(result.issues.size());
		message += result.issues.mid(0, 20).join(QStringLiteral("\n"));
		if (result.issues.size() > 20) {
			message += QStringLiteral("\n...");
		}
	}
	QMessageBox::information(this, tr("インポート完了"), message);
}

void LibraryManagerDialog::onImportPartFile() {
	const QString path = QFileDialog::getOpenFileName(this, tr("部品ファイルを取り込み"), QString(),
													   tr("部品ファイル (*.bpart *.part.json)"));
	if (path.isEmpty()) {
		return;
	}
	const auto result = m_libraryManager->importPartFile(path);
	if (!result.ok) {
		QMessageBox::warning(this, tr("インポート失敗"), result.error);
		return;
	}
	QMessageBox::information(this, tr("インポート完了"), tr("マイライブラリに取り込みました。"));
}

void LibraryManagerDialog::onImportBoardFile() {
	const QString path = QFileDialog::getOpenFileName(this, tr("基板ファイルを取り込み"), QString(),
													   tr("基板ファイル (*.bboard)"));
	if (path.isEmpty()) {
		return;
	}
	const auto result = m_libraryManager->importBoardFile(path);
	if (!result.ok) {
		QMessageBox::warning(this, tr("インポート失敗"), result.error);
		return;
	}
	QMessageBox::information(this, tr("インポート完了"), tr("マイライブラリに取り込みました。"));
}

// ---------------------------------------------------------------- 選択中の項目

void LibraryManagerDialog::onAddPart() {
	const QString libId = contextLibraryId();
	if (libId.isEmpty()) {
		return;
	}
	PartEditorDialog dialog(this);
	if (dialog.exec() != QDialog::Accepted) {
		return;
	}
	Part part = dialog.part();
	part.id = m_libraryManager->uniquePartId(libId, part.id);
	const auto result = m_libraryManager->addPartTo(libId, part, contextCategoryId());
	if (!result.ok) {
		QMessageBox::warning(this, tr("作成失敗"), result.error);
	}
}

void LibraryManagerDialog::onAddBoard() {
	const QString libId = contextLibraryId();
	if (libId.isEmpty()) {
		return;
	}
	BoardEditorDialog dialog(this);
	if (dialog.exec() != QDialog::Accepted) {
		return;
	}
	BoardSpec board = dialog.board();
	board.id = m_libraryManager->uniqueBoardId(libId, board.id);
	const auto result = m_libraryManager->addBoardTo(libId, board);
	if (!result.ok) {
		QMessageBox::warning(this, tr("作成失敗"), result.error);
	}
}

void LibraryManagerDialog::onCopyFromOtherLibrary() {
	const QString dstLibId = contextLibraryId();
	if (dstLibId.isEmpty()) {
		return;
	}
	PartPickerDialog dialog(m_libraryManager, dstLibId, this);
	if (dialog.exec() != QDialog::Accepted) {
		return;
	}
	const QString srcLibId = dialog.sourceLibraryId();
	const QStringList partIds = dialog.selectedPartIds();
	if (srcLibId.isEmpty() || partIds.isEmpty()) {
		return;
	}
	const auto srcLib = m_libraryManager->library(srcLibId);
	if (srcLib && !srcLib->redistribution.allowed) {
		const auto btn = QMessageBox::question(
			this, tr("確認"),
			tr("選択した部品は再配布が許可されていないライブラリ「%1」由来です。\n"
			   "複製先ライブラリを配布するとき、これらの部品は含められません。続けますか？")
				.arg(srcLib->name));
		if (btn != QMessageBox::Yes) {
			return;
		}
	}
	const auto result = m_libraryManager->copyPartsBetween(srcLibId, partIds, dstLibId, contextCategoryId());
	if (!result.ok) {
		QMessageBox::warning(this, tr("複製失敗"), result.error);
		return;
	}
	QMessageBox::information(this, tr("複製完了"), tr("%1件の部品を複製しました。").arg(result.partCount));
}

void LibraryManagerDialog::onEditSelected() {
	const Selection sel = currentSelection();
	const auto lib = m_libraryManager->library(sel.libId);
	if (!lib) {
		return;
	}
	if (sel.type == NodeType::Part) {
		const auto part = lib->part(sel.itemId);
		if (!part) return;
		PartEditorDialog dialog(this);
		dialog.setPart(*part);
		if (dialog.exec() != QDialog::Accepted) {
			return;
		}
		Part updated = dialog.part();
		updated.id = part->id;  // id は変えさせない (編集)
		const auto result = m_libraryManager->addPartTo(sel.libId, updated, lib->partCategory.value(part->id));
		if (!result.ok) {
			QMessageBox::warning(this, tr("保存失敗"), result.error);
		}
	} else if (sel.type == NodeType::Board) {
		const auto board = lib->board(sel.itemId);
		if (!board) return;
		BoardEditorDialog dialog(this);
		dialog.setBoard(*board);
		if (dialog.exec() != QDialog::Accepted) {
			return;
		}
		BoardSpec updated = dialog.board();
		updated.id = board->id;
		const auto result = m_libraryManager->addBoardTo(sel.libId, updated);
		if (!result.ok) {
			QMessageBox::warning(this, tr("保存失敗"), result.error);
		}
	}
}

void LibraryManagerDialog::onDeleteSelectedItem() {
	const Selection sel = currentSelection();
	const auto lib = m_libraryManager->library(sel.libId);
	if (!lib) {
		return;
	}
	if (sel.type == NodeType::Part) {
		const auto part = lib->part(sel.itemId);
		const auto btn = QMessageBox::question(this, tr("確認"),
											   tr("部品「%1」を削除します。よろしいですか？").arg(part ? part->name : sel.itemId));
		if (btn != QMessageBox::Yes) return;
		if (!m_libraryManager->removePartFrom(sel.libId, sel.itemId)) {
			QMessageBox::warning(this, tr("削除失敗"), tr("削除できませんでした。"));
		}
	} else if (sel.type == NodeType::Board) {
		const auto board = lib->board(sel.itemId);
		const auto btn = QMessageBox::question(
			this, tr("確認"), tr("基板「%1」を削除します。よろしいですか？").arg(board ? board->name : sel.itemId));
		if (btn != QMessageBox::Yes) return;
		if (!m_libraryManager->removeBoardFrom(sel.libId, sel.itemId)) {
			QMessageBox::warning(this, tr("削除失敗"), tr("削除できませんでした。"));
		}
	}
}

void LibraryManagerDialog::onChangeCategory() {
	const Selection sel = currentSelection();
	if (sel.type != NodeType::Part) {
		return;
	}
	const auto lib = m_libraryManager->library(sel.libId);
	if (!lib) {
		return;
	}
	QStringList names;
	QStringList ids;
	for (const auto &cat : lib->categories) {
		names << cat.name;
		ids << cat.id;
	}
	const QString currentCat = lib->partCategory.value(sel.itemId);
	const int currentIdx = ids.indexOf(currentCat);
	bool ok = false;
	const QString chosen =
		QInputDialog::getItem(this, tr("カテゴリを変更"), tr("カテゴリ:"), names, currentIdx >= 0 ? currentIdx : 0,
							  /*editable=*/false, &ok);
	if (!ok) {
		return;
	}
	const int idx = names.indexOf(chosen);
	if (idx < 0) {
		return;
	}
	if (!m_libraryManager->setPartCategory(sel.libId, sel.itemId, ids[idx])) {
		QMessageBox::warning(this, tr("変更失敗"), tr("カテゴリを変更できませんでした。"));
	}
}

void LibraryManagerDialog::onUseSelectedBoard() {
	const Selection sel = currentSelection();
	if (sel.type != NodeType::Board) {
		return;
	}
	const auto lib = m_libraryManager->library(sel.libId);
	const auto board = lib ? lib->board(sel.itemId) : nullptr;
	if (!board) {
		return;
	}
	emit boardApplyRequested(*board);
}

// ---------------------------------------------------------------- カテゴリ

void LibraryManagerDialog::onAddCategory() {
	const QString libId = contextLibraryId();
	if (libId.isEmpty()) {
		return;
	}
	CategoryEditDialog dialog(this);
	dialog.setWindowTitle(tr("カテゴリを追加"));
	if (dialog.exec() != QDialog::Accepted) {
		return;
	}
	const CategoryInfo cat = dialog.category();
	if (cat.id.isEmpty()) {
		QMessageBox::warning(this, tr("入力エラー"), tr("ID は必須です。"));
		return;
	}
	if (!m_libraryManager->addCategory(libId, cat)) {
		QMessageBox::warning(this, tr("追加失敗"), tr("同じ ID のカテゴリが既に存在します。"));
	}
}

void LibraryManagerDialog::onRenameCategory() {
	const Selection sel = currentSelection();
	if (sel.type != NodeType::Category) {
		return;
	}
	const auto lib = m_libraryManager->library(sel.libId);
	const auto *cat = lib ? lib->category(sel.itemId) : nullptr;
	if (!cat) {
		return;
	}
	CategoryEditDialog dialog(this);
	dialog.setWindowTitle(tr("カテゴリの名前を変更"));
	dialog.setCategory(*cat, /*idEditable=*/false);
	if (dialog.exec() != QDialog::Accepted) {
		return;
	}
	CategoryInfo updated = dialog.category();
	updated.id = cat->id;
	updated.icon = cat->icon;
	updated.order = cat->order;
	if (!m_libraryManager->updateCategory(sel.libId, updated)) {
		QMessageBox::warning(this, tr("変更失敗"), tr("名前を変更できませんでした。"));
	}
}

void LibraryManagerDialog::onDeleteCategory() {
	const Selection sel = currentSelection();
	if (sel.type != NodeType::Category || sel.itemId == LibraryManager::uncategorizedCategoryId()) {
		return;
	}
	const auto btn =
		QMessageBox::question(this, tr("確認"), tr("カテゴリを削除します。中の部品は「未分類」へ移動します。よろしいですか？"));
	if (btn != QMessageBox::Yes) {
		return;
	}
	if (!m_libraryManager->removeCategory(sel.libId, sel.itemId)) {
		QMessageBox::warning(this, tr("削除失敗"), tr("削除できませんでした。"));
	}
}
