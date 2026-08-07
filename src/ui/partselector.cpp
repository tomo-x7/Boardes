#include "partselector.h"

#include <QComboBox>
#include <QLineEdit>
#include <QListView>
#include <QVBoxLayout>
#include <algorithm>

#include "../model/librarymanager.h"

namespace {
constexpr int kIconSize = 48;
}

// ---------------------------------------------------------------- PartListModel

PartListModel::PartListModel(QObject *parent) : QAbstractListModel(parent) {
}

void PartListModel::setLibraryManager(LibraryManager *manager) {
	m_libraryManager = manager;
	rebuild();
}

void PartListModel::setFilter(PartSelectorMode mode, const QString &libraryId, const QString &categoryId,
							  const QString &query) {
	m_mode = mode;
	m_libraryFilter = libraryId;
	m_categoryFilter = categoryId;
	m_query = query;
	rebuild();
}

void PartListModel::rebuild() {
	beginResetModel();
	m_entries.clear();
	if (m_libraryManager) {
		const auto &libs = m_libraryManager->libraries();
		for (auto libIt = libs.constBegin(); libIt != libs.constEnd(); ++libIt) {
			const QString &libId = libIt.key();
			if (m_mode != PartSelectorMode::All && libId != m_libraryFilter) {
				continue;
			}
			const Library &lib = *libIt.value();
			for (auto partIt = lib.parts.constBegin(); partIt != lib.parts.constEnd(); ++partIt) {
				const QString &partId = partIt.key();
				const auto &part = partIt.value();
				const QString catId = lib.partCategory.value(partId);
				if (m_mode == PartSelectorMode::ByCategory && !m_categoryFilter.isEmpty() &&
					catId != m_categoryFilter) {
					continue;
				}
				if (!part->matchesQuery(m_query)) {
					continue;
				}
				m_entries.append({libId, partId, catId, part});
			}
		}
		std::sort(m_entries.begin(), m_entries.end(),
				 [](const Entry &a, const Entry &b) { return a.part->name < b.part->name; });
	}
	endResetModel();
}

int PartListModel::rowCount(const QModelIndex &parent) const {
	if (parent.isValid()) {
		return 0;
	}
	return m_entries.size();
}

QIcon PartListModel::iconFor(const Entry &entry) const {
	const QString key = entry.libraryId + QLatin1Char('/') + entry.partId;
	const auto it = m_iconCache.constFind(key);
	if (it != m_iconCache.constEnd()) {
		return it.value();
	}
	QIcon icon;
	if (entry.part && !entry.part->artwork.isNull()) {
		const QPixmap pm = QPixmap::fromImage(
			entry.part->artwork.image.scaled(kIconSize, kIconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
		icon = QIcon(pm);
	}
	m_iconCache.insert(key, icon);
	return icon;
}

QVariant PartListModel::data(const QModelIndex &index, int role) const {
	if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
		return {};
	}
	const Entry &e = m_entries[index.row()];
	switch (role) {
	case Qt::DisplayRole:
		return e.part->name;
	case Qt::DecorationRole:
		return iconFor(e);
	case Qt::ToolTipRole:
		return QStringLiteral("%1\n%2").arg(e.part->name, e.partId);
	case LibraryIdRole:
		return e.libraryId;
	case PartIdRole:
		return e.partId;
	case CategoryIdRole:
		return e.categoryId;
	default:
		return {};
	}
}

// ---------------------------------------------------------------- PartSelector

PartSelector::PartSelector(QWidget *parent) : QWidget(parent) {
	m_modeCombo = new QComboBox(this);
	m_modeCombo->addItem(QStringLiteral("ライブラリ別"), static_cast<int>(PartSelectorMode::ByLibrary));
	m_modeCombo->addItem(QStringLiteral("カテゴリ別"), static_cast<int>(PartSelectorMode::ByCategory));
	m_modeCombo->addItem(QStringLiteral("全部品"), static_cast<int>(PartSelectorMode::All));

	m_libraryCombo = new QComboBox(this);
	m_categoryCombo = new QComboBox(this);

	m_searchEdit = new QLineEdit(this);
	m_searchEdit->setPlaceholderText(QStringLiteral("部品を検索..."));
	m_searchEdit->setClearButtonEnabled(true);

	m_listView = new QListView(this);
	m_listView->setViewMode(QListView::IconMode);
	m_listView->setIconSize(QSize(kIconSize, kIconSize));
	m_listView->setResizeMode(QListView::Adjust);
	m_listView->setMovement(QListView::Static);
	m_listView->setSpacing(6);
	m_listView->setWordWrap(true);
	m_listView->setSelectionMode(QAbstractItemView::SingleSelection);
	m_listView->setUniformItemSizes(false);

	m_model = new PartListModel(this);
	m_listView->setModel(m_model);

	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(4, 4, 4, 4);
	layout->addWidget(m_modeCombo);
	layout->addWidget(m_libraryCombo);
	layout->addWidget(m_categoryCombo);
	layout->addWidget(m_searchEdit);
	layout->addWidget(m_listView, /*stretch=*/1);

	connect(m_modeCombo, &QComboBox::currentIndexChanged, this, &PartSelector::onModeChanged);
	connect(m_libraryCombo, &QComboBox::currentIndexChanged, this, &PartSelector::onLibraryComboChanged);
	connect(m_categoryCombo, &QComboBox::currentIndexChanged, this, &PartSelector::onCategoryComboChanged);
	connect(m_searchEdit, &QLineEdit::textChanged, this, &PartSelector::onSearchTextChanged);
	connect(m_listView, &QListView::activated, this, &PartSelector::onActivated);
	connect(m_listView->selectionModel(), &QItemSelectionModel::currentChanged, this,
			[this](const QModelIndex &current, const QModelIndex &) { onCurrentChanged(current); });

	updateFilterVisibility();
}

void PartSelector::setLibraryManager(LibraryManager *manager) {
	if (m_libraryManager) {
		disconnect(m_libraryManager, nullptr, this, nullptr);
	}
	m_libraryManager = manager;
	m_model->setLibraryManager(manager);
	if (m_libraryManager) {
		connect(m_libraryManager, &LibraryManager::librariesChanged, this, &PartSelector::onLibrariesChanged);
	}
	rebuildLibraryCombo();
	updateFilterVisibility();
	applyFilter();
}

PartSelectorMode PartSelector::currentMode() const {
	return static_cast<PartSelectorMode>(m_modeCombo->currentData().toInt());
}

void PartSelector::updateFilterVisibility() {
	const auto mode = currentMode();
	m_libraryCombo->setVisible(mode != PartSelectorMode::All);
	m_categoryCombo->setVisible(mode == PartSelectorMode::ByCategory);
}

void PartSelector::rebuildLibraryCombo() {
	const QString prev = m_libraryCombo->currentData().toString();
	m_libraryCombo->blockSignals(true);
	m_libraryCombo->clear();
	if (m_libraryManager) {
		const auto &libs = m_libraryManager->libraries();
		for (auto it = libs.constBegin(); it != libs.constEnd(); ++it) {
			m_libraryCombo->addItem(it.value()->name, it.key());
		}
	}
	const int idx = m_libraryCombo->findData(prev);
	m_libraryCombo->setCurrentIndex(idx >= 0 ? idx : (m_libraryCombo->count() > 0 ? 0 : -1));
	m_libraryCombo->blockSignals(false);
	rebuildCategoryCombo();
}

void PartSelector::rebuildCategoryCombo() {
	const QString prev = m_categoryCombo->currentData().toString();
	m_categoryCombo->blockSignals(true);
	m_categoryCombo->clear();
	m_categoryCombo->addItem(QStringLiteral("すべてのカテゴリ"), QString());
	if (m_libraryManager) {
		const QString libId = m_libraryCombo->currentData().toString();
		if (const auto lib = m_libraryManager->library(libId)) {
			QVector<CategoryInfo> cats = lib->categories;
			std::sort(cats.begin(), cats.end(),
					 [](const CategoryInfo &a, const CategoryInfo &b) { return a.order < b.order; });
			for (const auto &c : cats) {
				m_categoryCombo->addItem(c.icon.isNull() ? QIcon() : QIcon(QPixmap::fromImage(c.icon)), c.name,
										 c.id);
			}
		}
	}
	const int idx = m_categoryCombo->findData(prev);
	m_categoryCombo->setCurrentIndex(idx >= 0 ? idx : 0);
	m_categoryCombo->blockSignals(false);
}

void PartSelector::applyFilter() {
	const auto mode = currentMode();
	const QString libId = (mode != PartSelectorMode::All) ? m_libraryCombo->currentData().toString() : QString();
	const QString catId = (mode == PartSelectorMode::ByCategory) ? m_categoryCombo->currentData().toString() : QString();
	m_model->setFilter(mode, libId, catId, m_searchEdit->text());
}

void PartSelector::onModeChanged(int) {
	updateFilterVisibility();
	rebuildLibraryCombo();
	applyFilter();
}

void PartSelector::onLibraryComboChanged(int) {
	rebuildCategoryCombo();
	applyFilter();
}

void PartSelector::onCategoryComboChanged(int) {
	applyFilter();
}

void PartSelector::onSearchTextChanged(const QString &) {
	applyFilter();
}

void PartSelector::onActivated(const QModelIndex &index) {
	if (!index.isValid()) {
		return;
	}
	emit partActivated(index.data(PartListModel::LibraryIdRole).toString(),
					   index.data(PartListModel::PartIdRole).toString());
}

void PartSelector::onCurrentChanged(const QModelIndex &current) {
	if (!current.isValid()) {
		return;
	}
	emit partSelected(current.data(PartListModel::LibraryIdRole).toString(),
					  current.data(PartListModel::PartIdRole).toString());
}

void PartSelector::onLibrariesChanged() {
	rebuildLibraryCombo();
	applyFilter();
}
