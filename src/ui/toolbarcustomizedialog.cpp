#include "toolbarcustomizedialog.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QVBoxLayout>

#include "../core/ids.h"
#include "actionregistry.h"
#include "theme.h"

namespace {
constexpr int kSeparatorRole = Qt::UserRole + 1;
}

ToolbarCustomizeDialog::ToolbarCustomizeDialog(ActionRegistry *registry, QWidget *parent)
	: QDialog(parent), m_registry(registry) {
	setWindowTitle(tr("ツールバーのカスタマイズ"));
	resize(760, 480);
	m_layouts = toolbarlayout::load();

	auto *mainLayout = new QVBoxLayout(this);

	auto *toolbarRow = new QHBoxLayout();
	toolbarRow->addWidget(new QLabel(tr("対象ツールバー:"), this));
	m_toolbarCombo = new QComboBox(this);
	toolbarRow->addWidget(m_toolbarCombo, 1);
	auto *newButton = new QPushButton(tr("新しいツールバー..."), this);
	m_renameButton = new QPushButton(tr("名前変更..."), this);
	m_deleteButton = new QPushButton(tr("削除"), this);
	toolbarRow->addWidget(newButton);
	toolbarRow->addWidget(m_renameButton);
	toolbarRow->addWidget(m_deleteButton);
	mainLayout->addLayout(toolbarRow);

	m_builtinNoticeLabel = new QLabel(
		tr("既定のツールバーは表示/非表示と表示スタイルのみ変更できます。項目を自由に選びたい場合は"
		   "「新しいツールバー」を作成してください。"),
		this);
	m_builtinNoticeLabel->setWordWrap(true);
	m_builtinNoticeLabel->setStyleSheet(QStringLiteral("QLabel { color: palette(mid); }"));
	mainLayout->addWidget(m_builtinNoticeLabel);

	auto *listsRow = new QHBoxLayout();
	mainLayout->addLayout(listsRow, 1);

	auto *availableBox = new QGroupBox(tr("利用可能なコマンド"), this);
	auto *availableLayout = new QVBoxLayout(availableBox);
	m_availableList = new QListWidget(availableBox);
	m_availableList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	availableLayout->addWidget(m_availableList);
	listsRow->addWidget(availableBox, 1);

	auto *midButtonsLayout = new QVBoxLayout();
	m_addButton = new QPushButton(tr("追加 >"), this);
	m_removeButton = new QPushButton(tr("< 削除"), this);
	m_separatorButton = new QPushButton(tr("区切り線を追加"), this);
	midButtonsLayout->addStretch(1);
	midButtonsLayout->addWidget(m_addButton);
	midButtonsLayout->addWidget(m_removeButton);
	midButtonsLayout->addWidget(m_separatorButton);
	midButtonsLayout->addStretch(1);
	listsRow->addLayout(midButtonsLayout);

	auto *itemsBox = new QGroupBox(tr("ツールバーの内容"), this);
	auto *itemsLayout = new QVBoxLayout(itemsBox);
	m_itemsList = new QListWidget(itemsBox);
	itemsLayout->addWidget(m_itemsList);
	listsRow->addWidget(itemsBox, 1);

	auto *reorderLayout = new QVBoxLayout();
	m_upButton = new QPushButton(tr("↑"), this);
	m_downButton = new QPushButton(tr("↓"), this);
	reorderLayout->addStretch(1);
	reorderLayout->addWidget(m_upButton);
	reorderLayout->addWidget(m_downButton);
	reorderLayout->addStretch(1);
	listsRow->addLayout(reorderLayout);

	auto *optionsRow = new QHBoxLayout();
	m_visibleCheck = new QCheckBox(tr("このツールバーを表示する"), this);
	optionsRow->addWidget(m_visibleCheck);
	optionsRow->addWidget(new QLabel(tr("表示スタイル:"), this));
	m_styleCombo = new QComboBox(this);
	m_styleCombo->addItem(tr("アイコンのみ"), static_cast<int>(Qt::ToolButtonIconOnly));
	m_styleCombo->addItem(tr("文字のみ"), static_cast<int>(Qt::ToolButtonTextOnly));
	m_styleCombo->addItem(tr("アイコンと文字"), static_cast<int>(Qt::ToolButtonTextBesideIcon));
	m_styleCombo->addItem(tr("アイコンの下に文字"), static_cast<int>(Qt::ToolButtonTextUnderIcon));
	optionsRow->addWidget(m_styleCombo);
	optionsRow->addStretch(1);
	auto *resetAllButton = new QPushButton(tr("既定に戻す"), this);
	optionsRow->addWidget(resetAllButton);
	mainLayout->addLayout(optionsRow);

	auto *closeBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
	connect(closeBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
	mainLayout->addWidget(closeBox);

	connect(m_toolbarCombo, &QComboBox::currentIndexChanged, this, &ToolbarCustomizeDialog::onToolbarComboChanged);
	connect(newButton, &QPushButton::clicked, this, &ToolbarCustomizeDialog::onNewToolbar);
	connect(m_renameButton, &QPushButton::clicked, this, &ToolbarCustomizeDialog::onRenameToolbar);
	connect(m_deleteButton, &QPushButton::clicked, this, &ToolbarCustomizeDialog::onDeleteToolbar);
	connect(m_addButton, &QPushButton::clicked, this, &ToolbarCustomizeDialog::onAddItem);
	connect(m_removeButton, &QPushButton::clicked, this, &ToolbarCustomizeDialog::onRemoveItem);
	connect(m_upButton, &QPushButton::clicked, this, &ToolbarCustomizeDialog::onMoveUp);
	connect(m_downButton, &QPushButton::clicked, this, &ToolbarCustomizeDialog::onMoveDown);
	connect(m_separatorButton, &QPushButton::clicked, this, &ToolbarCustomizeDialog::onAddSeparator);
	connect(m_styleCombo, &QComboBox::currentIndexChanged, this, &ToolbarCustomizeDialog::onStyleChanged);
	connect(m_visibleCheck, &QCheckBox::toggled, this, &ToolbarCustomizeDialog::onVisibleToggled);
	connect(resetAllButton, &QPushButton::clicked, this, &ToolbarCustomizeDialog::onResetAll);

	rebuildToolbarCombo();
	Theme::suppressAutoDefault(this);
}

ToolbarLayout *ToolbarCustomizeDialog::currentLayout() {
	const int idx = m_toolbarCombo->currentIndex();
	if (idx < 0 || idx >= m_layouts.size()) {
		return nullptr;
	}
	return &m_layouts[idx];
}

QString ToolbarCustomizeDialog::labelFor(const QString &commandId) const {
	if (QAction *a = m_registry->action(commandId)) {
		QString text = a->text();
		text.remove(QLatin1Char('&'));
		if (!text.isEmpty()) {
			return text;
		}
	}
	return commandId;
}

void ToolbarCustomizeDialog::rebuildToolbarCombo() {
	const QString previousId = [this]() -> QString {
		if (auto *l = currentLayout()) return l->id;
		return QString();
	}();
	m_toolbarCombo->blockSignals(true);
	m_toolbarCombo->clear();
	for (const auto &layout : m_layouts) {
		m_toolbarCombo->addItem(layout.builtin ? tr("%1 (既定)").arg(layout.title) : layout.title);
	}
	int restoreIndex = 0;
	for (int i = 0; i < m_layouts.size(); ++i) {
		if (m_layouts[i].id == previousId) {
			restoreIndex = i;
			break;
		}
	}
	if (m_toolbarCombo->count() > 0) {
		m_toolbarCombo->setCurrentIndex(restoreIndex);
	}
	m_toolbarCombo->blockSignals(false);
	onToolbarComboChanged(m_toolbarCombo->currentIndex());
}

void ToolbarCustomizeDialog::onToolbarComboChanged(int) {
	updateEditableState();
	rebuildAvailableAndItemsLists();
	if (auto *layout = currentLayout()) {
		m_visibleCheck->blockSignals(true);
		m_visibleCheck->setChecked(layout->visible);
		m_visibleCheck->blockSignals(false);
		m_styleCombo->blockSignals(true);
		const int styleIdx = m_styleCombo->findData(static_cast<int>(layout->style));
		m_styleCombo->setCurrentIndex(styleIdx >= 0 ? styleIdx : 2);
		m_styleCombo->blockSignals(false);
	}
}

void ToolbarCustomizeDialog::updateEditableState() {
	const auto *layout = currentLayout();
	const bool isCustom = layout && !layout->builtin;
	m_renameButton->setEnabled(isCustom);
	m_deleteButton->setEnabled(isCustom);
	m_addButton->setEnabled(isCustom);
	m_removeButton->setEnabled(isCustom);
	m_upButton->setEnabled(isCustom);
	m_downButton->setEnabled(isCustom);
	m_separatorButton->setEnabled(isCustom);
	m_availableList->setEnabled(isCustom);
	m_itemsList->setEnabled(layout != nullptr);
}

void ToolbarCustomizeDialog::rebuildAvailableAndItemsLists() {
	m_availableList->clear();
	m_itemsList->clear();
	const auto *layout = currentLayout();
	if (!layout) {
		return;
	}
	QSet<QString> used;
	for (const auto &item : layout->items) {
		if (item == QStringLiteral("-")) {
			auto *row = new QListWidgetItem(tr("── 区切り線 ──"), m_itemsList);
			row->setData(kSeparatorRole, true);
			continue;
		}
		used.insert(item);
		auto *row = new QListWidgetItem(labelFor(item), m_itemsList);
		row->setData(Qt::UserRole, item);
	}
	for (const QString &id : m_registry->commandIds()) {
		if (used.contains(id)) {
			continue;
		}
		auto *row = new QListWidgetItem(labelFor(id), m_availableList);
		row->setData(Qt::UserRole, id);
	}
}

void ToolbarCustomizeDialog::persistAndNotify() {
	toolbarlayout::save(m_layouts);
	emit layoutsChanged();
}

void ToolbarCustomizeDialog::onNewToolbar() {
	bool ok = false;
	const QString title =
		QInputDialog::getText(this, tr("新しいツールバー"), tr("ツールバー名:"), QLineEdit::Normal, QString(), &ok);
	if (!ok || title.trimmed().isEmpty()) {
		return;
	}
	ToolbarLayout layout;
	layout.id = ids::newUuid();
	layout.title = title.trimmed();
	layout.builtin = false;
	layout.visible = true;
	layout.style = Qt::ToolButtonTextBesideIcon;
	m_layouts.append(layout);
	persistAndNotify();
	rebuildToolbarCombo();
	m_toolbarCombo->setCurrentIndex(m_layouts.size() - 1);
}

void ToolbarCustomizeDialog::onRenameToolbar() {
	auto *layout = currentLayout();
	if (!layout || layout->builtin) {
		return;
	}
	bool ok = false;
	const QString title =
		QInputDialog::getText(this, tr("名前変更"), tr("ツールバー名:"), QLineEdit::Normal, layout->title, &ok);
	if (!ok || title.trimmed().isEmpty()) {
		return;
	}
	layout->title = title.trimmed();
	persistAndNotify();
	rebuildToolbarCombo();
}

void ToolbarCustomizeDialog::onDeleteToolbar() {
	auto *layout = currentLayout();
	if (!layout || layout->builtin) {
		return;
	}
	if (QMessageBox::question(this, tr("ツールバーを削除"), tr("「%1」を削除しますか？").arg(layout->title)) !=
		QMessageBox::Yes) {
		return;
	}
	const QString id = layout->id;
	m_layouts.removeIf([&id](const ToolbarLayout &l) { return l.id == id; });
	persistAndNotify();
	rebuildToolbarCombo();
}

void ToolbarCustomizeDialog::onAddItem() {
	auto *layout = currentLayout();
	if (!layout || layout->builtin) {
		return;
	}
	for (QListWidgetItem *item : m_availableList->selectedItems()) {
		layout->items << item->data(Qt::UserRole).toString();
	}
	persistAndNotify();
	rebuildAvailableAndItemsLists();
}

void ToolbarCustomizeDialog::onRemoveItem() {
	auto *layout = currentLayout();
	const int row = m_itemsList->currentRow();
	if (!layout || layout->builtin || row < 0 || row >= layout->items.size()) {
		return;
	}
	layout->items.removeAt(row);
	persistAndNotify();
	rebuildAvailableAndItemsLists();
}

void ToolbarCustomizeDialog::onMoveUp() {
	auto *layout = currentLayout();
	const int row = m_itemsList->currentRow();
	if (!layout || layout->builtin || row <= 0 || row >= layout->items.size()) {
		return;
	}
	layout->items.swapItemsAt(row, row - 1);
	persistAndNotify();
	rebuildAvailableAndItemsLists();
	m_itemsList->setCurrentRow(row - 1);
}

void ToolbarCustomizeDialog::onMoveDown() {
	auto *layout = currentLayout();
	const int row = m_itemsList->currentRow();
	if (!layout || layout->builtin || row < 0 || row >= layout->items.size() - 1) {
		return;
	}
	layout->items.swapItemsAt(row, row + 1);
	persistAndNotify();
	rebuildAvailableAndItemsLists();
	m_itemsList->setCurrentRow(row + 1);
}

void ToolbarCustomizeDialog::onAddSeparator() {
	auto *layout = currentLayout();
	if (!layout || layout->builtin) {
		return;
	}
	layout->items << QStringLiteral("-");
	persistAndNotify();
	rebuildAvailableAndItemsLists();
}

void ToolbarCustomizeDialog::onStyleChanged(int) {
	auto *layout = currentLayout();
	if (!layout) {
		return;
	}
	layout->style = static_cast<Qt::ToolButtonStyle>(m_styleCombo->currentData().toInt());
	persistAndNotify();
}

void ToolbarCustomizeDialog::onVisibleToggled(bool checked) {
	auto *layout = currentLayout();
	if (!layout) {
		return;
	}
	layout->visible = checked;
	persistAndNotify();
}

void ToolbarCustomizeDialog::onResetAll() {
	if (QMessageBox::question(this, tr("既定に戻す"),
							  tr("すべてのツールバー構成を既定値に戻します (作成したツールバーは失われます)。"
								 "よろしいですか？")) != QMessageBox::Yes) {
		return;
	}
	m_layouts = toolbarlayout::defaults();
	persistAndNotify();
	rebuildToolbarCombo();
}
