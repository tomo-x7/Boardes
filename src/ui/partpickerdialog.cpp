#include "partpickerdialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QVBoxLayout>

#include "../model/librarymanager.h"
#include "partselector.h"

namespace {
constexpr int kIconSize = 40;
}

PartPickerDialog::PartPickerDialog(LibraryManager *libraryManager, const QString &excludeLibraryId, QWidget *parent)
	: QDialog(parent), m_libraryManager(libraryManager), m_excludeLibraryId(excludeLibraryId) {
	setWindowTitle(tr("他のライブラリから部品を複製"));

	m_libraryCombo = new QComboBox(this);
	m_searchEdit = new QLineEdit(this);
	m_searchEdit->setPlaceholderText(tr("部品を検索..."));
	m_searchEdit->setClearButtonEnabled(true);

	m_listView = new QListView(this);
	m_listView->setViewMode(QListView::IconMode);
	m_listView->setIconSize(QSize(kIconSize, kIconSize));
	m_listView->setResizeMode(QListView::Adjust);
	m_listView->setSpacing(6);
	m_listView->setWordWrap(true);
	m_listView->setSelectionMode(QAbstractItemView::ExtendedSelection);

	m_model = new PartListModel(this);
	m_model->setLibraryManager(m_libraryManager);
	m_listView->setModel(m_model);

	m_countLabel = new QLabel(tr("0件選択"), this);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	auto *layout = new QVBoxLayout(this);
	layout->addWidget(new QLabel(tr("複製元ライブラリ:"), this));
	layout->addWidget(m_libraryCombo);
	layout->addWidget(m_searchEdit);
	layout->addWidget(m_listView, /*stretch=*/1);
	layout->addWidget(m_countLabel);
	layout->addWidget(buttons);
	resize(480, 520);

	connect(m_libraryCombo, &QComboBox::currentIndexChanged, this, &PartPickerDialog::onLibraryComboChanged);
	connect(m_searchEdit, &QLineEdit::textChanged, this, &PartPickerDialog::onSearchTextChanged);

	rebuildLibraryCombo();
	applyFilter();

	connect(m_listView->selectionModel(), &QItemSelectionModel::selectionChanged, this,
			&PartPickerDialog::onSelectionChanged);
}

void PartPickerDialog::rebuildLibraryCombo() {
	m_libraryCombo->blockSignals(true);
	m_libraryCombo->clear();
	if (m_libraryManager) {
		const auto &libs = m_libraryManager->libraries();
		for (auto it = libs.constBegin(); it != libs.constEnd(); ++it) {
			if (it.key() == m_excludeLibraryId) {
				continue;
			}
			m_libraryCombo->addItem(it.value()->name, it.key());
		}
	}
	m_libraryCombo->blockSignals(false);
}

void PartPickerDialog::applyFilter() {
	const QString libId = m_libraryCombo->currentData().toString();
	m_model->setFilter(PartSelectorMode::ByLibrary, libId, QString(), m_searchEdit->text());
}

void PartPickerDialog::onLibraryComboChanged(int) {
	applyFilter();
}

void PartPickerDialog::onSearchTextChanged(const QString &) {
	applyFilter();
}

void PartPickerDialog::onSelectionChanged() {
	m_countLabel->setText(tr("%1件選択").arg(m_listView->selectionModel()->selectedIndexes().size()));
}

QString PartPickerDialog::sourceLibraryId() const {
	return m_libraryCombo->currentData().toString();
}

QStringList PartPickerDialog::selectedPartIds() const {
	QStringList ids;
	for (const QModelIndex &idx : m_listView->selectionModel()->selectedIndexes()) {
		ids << idx.data(PartListModel::PartIdRole).toString();
	}
	return ids;
}
