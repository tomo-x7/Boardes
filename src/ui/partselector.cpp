#include "partselector.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>
#include <algorithm>

#include "../model/librarymanager.h"
#include "helphint.h"

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
			// libraryFilter が空なら「すべてのライブラリ」を意味する (mode==All のときは
			// 元々常にそう。mode==ByCategory でも「すべてのライブラリ」を選べるようにした
			// ため、空判定で分岐する — 個別のライブラリ名が空文字列になることは無い)。
			if (!m_libraryFilter.isEmpty() && libId != m_libraryFilter) {
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
	helphint::attach(m_modeCombo, QStringLiteral("部品一覧の絞り込み方法を選びます。"));

	m_libraryCombo = new QComboBox(this);
	helphint::attach(m_libraryCombo, QStringLiteral("表示するライブラリを選びます。"));
	m_categoryCombo = new QComboBox(this);
	helphint::attach(m_categoryCombo, QStringLiteral("表示するカテゴリを選びます。"));

	m_searchEdit = new QLineEdit(this);
	m_searchEdit->setPlaceholderText(QStringLiteral("部品を検索..."));
	m_searchEdit->setClearButtonEnabled(true);
	helphint::attach(m_searchEdit, QStringLiteral("部品名・キーワードで絞り込みます。"));

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

	m_pendingIcon = new QLabel(this);
	m_pendingIcon->setFixedSize(20, 20);
	m_pendingIcon->setScaledContents(true);
	m_pendingLabel = new QLabel(this);
	m_pendingClearButton = new QPushButton(tr("解除"), this);
	connect(m_pendingClearButton, &QPushButton::clicked, this, &PartSelector::clearPendingPart);
	auto *pendingLayout = new QHBoxLayout();
	pendingLayout->addWidget(new QLabel(tr("配置中:"), this));
	pendingLayout->addWidget(m_pendingIcon);
	pendingLayout->addWidget(m_pendingLabel, /*stretch=*/1);
	pendingLayout->addWidget(m_pendingClearButton);
	m_pendingBar = new QWidget(this);
	m_pendingBar->setLayout(pendingLayout);
	m_pendingBar->setStyleSheet(
		QStringLiteral("QWidget { background: palette(highlight); border-radius: 3px; }"
					   "QLabel { color: palette(highlighted-text); }"));
	m_pendingBar->setVisible(false);

	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(4, 4, 4, 4);
	layout->addWidget(m_modeCombo);
	layout->addWidget(m_libraryCombo);
	layout->addWidget(m_categoryCombo);
	layout->addWidget(m_searchEdit);
	layout->addWidget(m_pendingBar);
	layout->addWidget(m_listView, /*stretch=*/1);

	connect(m_modeCombo, &QComboBox::currentIndexChanged, this, &PartSelector::onModeChanged);
	connect(m_libraryCombo, &QComboBox::currentIndexChanged, this, &PartSelector::onLibraryComboChanged);
	connect(m_categoryCombo, &QComboBox::currentIndexChanged, this, &PartSelector::onCategoryComboChanged);
	connect(m_searchEdit, &QLineEdit::textChanged, this, &PartSelector::onSearchTextChanged);
	// シングルクリック (キーボード操作含む) で選択 = 配置対象になる。ダブルクリックは
	// 要求しない (以前はダブルクリックでのみ配置ツールに入れたが使いにくいとの指摘のため)。
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
	// カテゴリ別モードだけ「すべてのライブラリ」を選べる (同じ id のカテゴリを横断して
	// 一覧できると便利なため)。ライブラリ別モードでは元々「全部品」モードが別にあるので
	// 不要。
	if (currentMode() == PartSelectorMode::ByCategory) {
		m_libraryCombo->addItem(QStringLiteral("すべてのライブラリ"), QString());
	}
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
		if (!libId.isEmpty()) {
			if (const auto lib = m_libraryManager->library(libId)) {
				QVector<CategoryInfo> cats = lib->categories;
				std::sort(cats.begin(), cats.end(),
						 [](const CategoryInfo &a, const CategoryInfo &b) { return a.order < b.order; });
				for (const auto &c : cats) {
					m_categoryCombo->addItem(c.icon.isNull() ? QIcon() : QIcon(QPixmap::fromImage(c.icon)), c.name,
											 c.id);
				}
			}
		} else if (currentMode() == PartSelectorMode::ByCategory) {
			// 「すべてのライブラリ」: 全ライブラリのカテゴリを id で統合する
			// (同じ id のカテゴリは1行にまとめ、表示名は最初に見つかったものを使う)。
			QVector<CategoryInfo> merged;
			const auto &libs = m_libraryManager->libraries();
			for (auto libIt = libs.constBegin(); libIt != libs.constEnd(); ++libIt) {
				for (const auto &c : libIt.value()->categories) {
					const bool alreadyPresent =
						std::any_of(merged.begin(), merged.end(), [&](const CategoryInfo &m) { return m.id == c.id; });
					if (!alreadyPresent) {
						merged.append(c);
					}
				}
			}
			std::sort(merged.begin(), merged.end(),
					 [](const CategoryInfo &a, const CategoryInfo &b) { return a.order < b.order; });
			for (const auto &c : merged) {
				m_categoryCombo->addItem(c.icon.isNull() ? QIcon() : QIcon(QPixmap::fromImage(c.icon)), c.name, c.id);
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

void PartSelector::onCurrentChanged(const QModelIndex &current) {
	if (!current.isValid()) {
		m_pendingBar->setVisible(false);
		return;
	}
	const QString name = current.data(Qt::DisplayRole).toString();
	const QIcon icon = current.data(Qt::DecorationRole).value<QIcon>();
	m_pendingLabel->setText(name);
	if (!icon.isNull()) {
		m_pendingIcon->setPixmap(icon.pixmap(20, 20));
	} else {
		m_pendingIcon->setPixmap(QPixmap());
	}
	m_pendingBar->setVisible(true);
	emit partSelected(current.data(PartListModel::LibraryIdRole).toString(),
					  current.data(PartListModel::PartIdRole).toString());
}

void PartSelector::clearPendingPart() {
	m_listView->clearSelection();
	m_listView->selectionModel()->clearCurrentIndex();
	m_pendingBar->setVisible(false);
}

void PartSelector::onLibrariesChanged() {
	rebuildLibraryCombo();
	applyFilter();
}
