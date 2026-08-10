#include "keymapdialog.h"

#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "commandregistry.h"
#include "gesturecapturedialog.h"
#include "keymap.h"
#include "../theme.h"

namespace {

QString categoryDisplayName(const QString &category) {
	static const QHash<QString, QString> names = {
		{QStringLiteral("global"), QObject::tr("全体")},   {QStringLiteral("select"), QObject::tr("選択ツール")},
		{QStringLiteral("place"), QObject::tr("配置ツール")}, {QStringLiteral("wire"), QObject::tr("配線ツール")},
		{QStringLiteral("draft"), QObject::tr("下書きツール")}, {QStringLiteral("view"), QObject::tr("ビュー")},
	};
	return names.value(category, category);
}

}  // namespace

KeymapDialog::KeymapDialog(Keymap *keymap, QWidget *parent) : QDialog(parent), m_keymap(keymap) {
	setWindowTitle(tr("操作のカスタマイズ"));
	resize(720, 480);

	auto *mainLayout = new QVBoxLayout(this);
	auto *hLayout = new QHBoxLayout();
	mainLayout->addLayout(hLayout, 1);

	m_categoryList = new QListWidget(this);
	m_categoryList->setMaximumWidth(160);
	hLayout->addWidget(m_categoryList);

	m_commandTree = new QTreeWidget(this);
	m_commandTree->setColumnCount(2);
	m_commandTree->setHeaderLabels({tr("コマンド"), tr("割り当て")});
	m_commandTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	m_commandTree->setRootIsDecorated(false);
	hLayout->addWidget(m_commandTree, 1);

	auto *rightLayout = new QVBoxLayout();
	hLayout->addLayout(rightLayout);
	rightLayout->addWidget(new QLabel(tr("選択したコマンドへの割り当て:"), this));
	m_gestureTree = new QTreeWidget(this);
	m_gestureTree->setColumnCount(1);
	m_gestureTree->setHeaderLabels({tr("割り当て")});
	m_gestureTree->setMinimumWidth(200);
	rightLayout->addWidget(m_gestureTree, 1);

	auto *addButton = new QPushButton(tr("割り当てを追加..."), this);
	m_removeButton = new QPushButton(tr("選択した割り当てを削除"), this);
	m_resetButton = new QPushButton(tr("既定に戻す"), this);
	rightLayout->addWidget(addButton);
	rightLayout->addWidget(m_removeButton);
	rightLayout->addWidget(m_resetButton);
	connect(addButton, &QPushButton::clicked, this, &KeymapDialog::onAddGesture);
	connect(m_removeButton, &QPushButton::clicked, this, &KeymapDialog::onRemoveGesture);
	connect(m_resetButton, &QPushButton::clicked, this, &KeymapDialog::onResetCommand);

	m_conflictLabel = new QLabel(this);
	m_conflictLabel->setWordWrap(true);
	// 仕様書§3.2のerrorトークン (ライト/ダークで値が異なる)。重複割り当ての警告は保存を
	// ブロックしない旨をここで表す。
	m_conflictLabel->setStyleSheet(
		QStringLiteral("QLabel { color: %1; }").arg(Theme::instance().errorColor().name()));
	mainLayout->addWidget(m_conflictLabel);

	auto *bottomLayout = new QHBoxLayout();
	auto *resetAllButton = new QPushButton(tr("すべて既定に戻す"), this);
	auto *exportButton = new QPushButton(tr("書き出し(JSON)..."), this);
	auto *importButton = new QPushButton(tr("読み込み(JSON)..."), this);
	connect(resetAllButton, &QPushButton::clicked, this, &KeymapDialog::onResetAll);
	connect(exportButton, &QPushButton::clicked, this, &KeymapDialog::onExportJson);
	connect(importButton, &QPushButton::clicked, this, &KeymapDialog::onImportJson);
	bottomLayout->addWidget(resetAllButton);
	bottomLayout->addWidget(exportButton);
	bottomLayout->addWidget(importButton);
	bottomLayout->addStretch(1);
	auto *closeBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
	connect(closeBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(closeBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
	bottomLayout->addWidget(closeBox);
	mainLayout->addLayout(bottomLayout);

	connect(m_categoryList, &QListWidget::currentRowChanged, this, &KeymapDialog::onCategorySelectionChanged);
	connect(m_commandTree, &QTreeWidget::currentItemChanged, this, &KeymapDialog::onCommandSelectionChanged);
	// rebuildGestureTree() のたびにここで繋ぎ直すのではなく、コンストラクタで1回だけ
	// 繋ぐ (Qt::UniqueConnection はメンバ関数ポインタ以外のスロットには使えず、
	// ラムダと組み合わせると "unique connections require a pointer to member function"
	// で即座に fatal してしまうため — 実際に踏んだバグ)。
	connect(m_gestureTree, &QTreeWidget::currentItemChanged, this, &KeymapDialog::onGestureSelectionChanged);
	connect(m_keymap, &Keymap::changed, this, [this] {
		rebuildCommandTree();
		rebuildGestureTree();
		updateConflictLabel();
	});

	rebuildCategoryList();
	updateConflictLabel();
	Theme::suppressAutoDefault(this);
}

void KeymapDialog::rebuildCategoryList() {
	m_categoryList->clear();
	QStringList seen;
	for (const auto &cmd : commandregistry::allCommands()) {
		if (!seen.contains(cmd.category)) {
			seen << cmd.category;
			m_categoryList->addItem(categoryDisplayName(cmd.category));
		}
	}
	if (m_categoryList->count() > 0) {
		m_categoryList->setCurrentRow(0);
	}
}

void KeymapDialog::onCategorySelectionChanged() {
	rebuildCommandTree();
}

void KeymapDialog::rebuildCommandTree() {
	const int row = m_categoryList->currentRow();
	QStringList categories;
	for (const auto &cmd : commandregistry::allCommands()) {
		if (!categories.contains(cmd.category)) {
			categories << cmd.category;
		}
	}
	const QString category = (row >= 0 && row < categories.size()) ? categories[row] : QString();

	const QString previousId = currentCommandId();
	m_commandTree->clear();
	for (const auto &cmd : commandregistry::allCommands()) {
		if (cmd.category != category) {
			continue;
		}
		auto *item = new QTreeWidgetItem(m_commandTree, {cmd.label, m_keymap->displayFor(cmd.id)});
		item->setData(0, Qt::UserRole, cmd.id);
		item->setToolTip(0, cmd.description);
		if (cmd.id == previousId) {
			m_commandTree->setCurrentItem(item);
		}
	}
	if (!m_commandTree->currentItem() && m_commandTree->topLevelItemCount() > 0) {
		m_commandTree->setCurrentItem(m_commandTree->topLevelItem(0));
	}
	rebuildGestureTree();
}

QString KeymapDialog::currentCommandId() const {
	if (auto *item = m_commandTree->currentItem()) {
		return item->data(0, Qt::UserRole).toString();
	}
	return QString();
}

void KeymapDialog::onCommandSelectionChanged() {
	rebuildGestureTree();
}

void KeymapDialog::rebuildGestureTree() {
	m_gestureTree->clear();
	const QString id = currentCommandId();
	const bool hasCommand = !id.isEmpty();
	m_removeButton->setEnabled(false);
	m_resetButton->setEnabled(hasCommand && m_keymap->isCustomized(id));
	if (!hasCommand) {
		return;
	}
	for (const auto &g : m_keymap->gesturesFor(id)) {
		new QTreeWidgetItem(m_gestureTree, {g.toDisplayString()});
	}
}

void KeymapDialog::onGestureSelectionChanged() {
	m_removeButton->setEnabled(m_gestureTree->currentItem() != nullptr);
}

void KeymapDialog::onAddGesture() {
	const QString id = currentCommandId();
	if (id.isEmpty()) {
		return;
	}
	const auto *def = commandregistry::find(id);
	const InputKind kind = (def && !def->defaults.isEmpty()) ? def->defaults.first().kind : InputKind::Key;
	GestureCaptureDialog dlg(kind, this);
	if (dlg.exec() != QDialog::Accepted || !dlg.capturedGesture().has_value()) {
		return;
	}
	QVector<InputGesture> gestures = m_keymap->gesturesFor(id);
	gestures.append(*dlg.capturedGesture());
	m_keymap->setGestures(id, gestures);
}

void KeymapDialog::onRemoveGesture() {
	const QString id = currentCommandId();
	const int row = m_gestureTree->indexOfTopLevelItem(m_gestureTree->currentItem());
	if (id.isEmpty() || row < 0) {
		return;
	}
	QVector<InputGesture> gestures = m_keymap->gesturesFor(id);
	if (row >= gestures.size()) {
		return;
	}
	gestures.remove(row);
	m_keymap->setGestures(id, gestures);
}

void KeymapDialog::onResetCommand() {
	const QString id = currentCommandId();
	if (!id.isEmpty()) {
		m_keymap->reset(id);
	}
}

void KeymapDialog::onResetAll() {
	if (QMessageBox::question(this, tr("すべて既定に戻す"), tr("すべての操作割り当てを既定値に戻します。よろしいですか？")) !=
		QMessageBox::Yes) {
		return;
	}
	m_keymap->resetAll();
}

void KeymapDialog::updateConflictLabel() {
	const auto conflicts = m_keymap->conflicts();
	if (conflicts.isEmpty()) {
		m_conflictLabel->clear();
		return;
	}
	QStringList lines;
	for (const auto &c : conflicts) {
		const auto *defA = commandregistry::find(c.commandIdA);
		const auto *defB = commandregistry::find(c.commandIdB);
		lines << tr("「%1」と「%2」が同じ操作 (%3) に割り当てられています。")
					 .arg(defA ? defA->label : c.commandIdA, defB ? defB->label : c.commandIdB,
						  c.gesture.toDisplayString());
	}
	m_conflictLabel->setText(lines.join(QLatin1Char('\n')));
}

void KeymapDialog::onExportJson() {
	const QString path = QFileDialog::getSaveFileName(this, tr("操作割り当てを書き出し"), QString(), tr("JSON (*.json)"));
	if (path.isEmpty()) {
		return;
	}
	QJsonObject root;
	for (const QString &id : m_keymap->customizedCommandIds()) {
		QJsonArray arr;
		for (const auto &g : m_keymap->gesturesFor(id)) {
			arr.append(g.toStorageString());
		}
		root[id] = arr;
	}
	QFile f(path);
	if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		QMessageBox::warning(this, tr("書き出しに失敗しました"), path);
		return;
	}
	f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

void KeymapDialog::onImportJson() {
	const QString path = QFileDialog::getOpenFileName(this, tr("操作割り当てを読み込み"), QString(), tr("JSON (*.json)"));
	if (path.isEmpty()) {
		return;
	}
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly)) {
		QMessageBox::warning(this, tr("読み込みに失敗しました"), path);
		return;
	}
	QJsonParseError err;
	const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
	if (err.error != QJsonParseError::NoError || !doc.isObject()) {
		QMessageBox::warning(this, tr("読み込みに失敗しました"), tr("JSON として解析できません: %1").arg(err.errorString()));
		return;
	}
	const QJsonObject root = doc.object();
	for (auto it = root.constBegin(); it != root.constEnd(); ++it) {
		if (!commandregistry::find(it.key())) {
			continue;  // 未知の commandId は無視する (前方互換)
		}
		QVector<InputGesture> gestures;
		for (const auto &v : it.value().toArray()) {
			if (const auto g = InputGesture::fromStorageString(v.toString()); g.has_value()) {
				gestures.append(*g);
			}
		}
		if (!gestures.isEmpty()) {
			m_keymap->setGestures(it.key(), gestures);
		}
	}
}
