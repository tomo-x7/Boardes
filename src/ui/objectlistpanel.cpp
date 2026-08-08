#include "objectlistpanel.h"

#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QMenu>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

#include "../commands/placementcommands.h"
#include "../commands/wirecommands.h"
#include "../model/document.h"
#include "../model/librarymanager.h"
#include "../model/part.h"
#include "../render/boardscene.h"
#include "../render/boardview.h"
#include "../render/items/placementitem.h"
#include "../render/items/wireitem.h"
#include "../ui/tools/toolcontext.h"
#include "helphint.h"

namespace {
constexpr int kKindRole = Qt::UserRole;
constexpr int kUuidRole = Qt::UserRole + 1;
enum Kind { KindPlacementRoot, KindPlacement, KindWireRoot, KindWire };

QString wireLayerDisplayName(WireLayer layer) {
	switch (layer) {
	case WireLayer::FrontBare:
		return QObject::tr("表面裸線");
	case WireLayer::BackBare:
		return QObject::tr("裏面裸線");
	case WireLayer::FrontInsulated:
		return QObject::tr("表面被覆配線");
	case WireLayer::BackInsulated:
		return QObject::tr("裏面被覆配線");
	case WireLayer::Outline:
	default:
		return QObject::tr("外形線");
	}
}
}  // namespace

ObjectListPanel::ObjectListPanel(QWidget *parent) : QWidget(parent) {
	m_filterEdit = new QLineEdit(this);
	m_filterEdit->setPlaceholderText(tr("絞り込み (refDes・値・部品名・レイヤ名)"));
	m_filterEdit->setClearButtonEnabled(true);

	m_tree = new QTreeWidget(this);
	m_tree->setColumnCount(2);
	m_tree->setHeaderLabels({tr("表示"), tr("要素")});
	m_tree->header()->setStretchLastSection(true);
	m_tree->setRootIsDecorated(true);
	m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
	m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);

	auto *headerRow = new QHBoxLayout();
	headerRow->addWidget(m_filterEdit);
	headerRow->addWidget(helphint::button(
		tr("現在の設計データに含まれる部品・配線の一覧です。目玉アイコンで表示/非表示を切り替え、"
		   "ダブルクリックでその位置へジャンプできます。右クリックで削除・回転などの操作ができます。"),
		this));

	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(4, 4, 4, 4);
	layout->addLayout(headerRow);
	layout->addWidget(m_tree, /*stretch=*/1);

	connect(m_filterEdit, &QLineEdit::textChanged, this, &ObjectListPanel::onFilterTextChanged);
	connect(m_tree, &QTreeWidget::itemChanged, this, &ObjectListPanel::onItemChanged);
	connect(m_tree, &QTreeWidget::currentItemChanged, this, &ObjectListPanel::onCurrentItemChanged);
	connect(m_tree, &QTreeWidget::itemDoubleClicked, this, &ObjectListPanel::onItemDoubleClicked);
	connect(m_tree, &QTreeWidget::customContextMenuRequested, this, &ObjectListPanel::onContextMenuRequested);
}

void ObjectListPanel::setContext(ToolContext *ctx) {
	if (m_ctx && m_ctx->frontScene) disconnect(m_ctx->frontScene, nullptr, this, nullptr);
	if (m_ctx && m_ctx->backScene) disconnect(m_ctx->backScene, nullptr, this, nullptr);
	m_ctx = ctx;
	if (m_ctx && m_ctx->frontScene) {
		connect(m_ctx->frontScene, &QGraphicsScene::selectionChanged, this, &ObjectListPanel::onFrontSelectionChanged);
	}
	if (m_ctx && m_ctx->backScene) {
		connect(m_ctx->backScene, &QGraphicsScene::selectionChanged, this, &ObjectListPanel::onBackSelectionChanged);
	}
	refresh();
}

void ObjectListPanel::refresh() {
	rebuildTree();
}

void ObjectListPanel::rebuildTree() {
	m_rebuilding = true;
	m_tree->clear();
	if (!m_ctx || !m_ctx->document) {
		m_rebuilding = false;
		return;
	}
	const QString filter = m_filterEdit->text();
	Document *doc = m_ctx->document;

	auto *placementRoot = new QTreeWidgetItem(m_tree);
	placementRoot->setText(1, tr("部品 (%1)").arg(doc->placements.size()));
	placementRoot->setData(1, kKindRole, KindPlacementRoot);
	placementRoot->setFlags(placementRoot->flags() & ~Qt::ItemIsUserCheckable);

	for (const auto &p : doc->placements) {
		QString partName = p->partId;
		if (m_ctx->libraryManager) {
			if (const auto part = m_ctx->libraryManager->resolvePart(p->libraryId, p->partId)) {
				partName = part->name;
			}
		}
		const QString label = tr("%1  %2  %3面  (%4,%5)  %6°")
								   .arg(p->refDes.isEmpty() ? tr("(No)") : p->refDes, partName,
										p->side == Side::Front ? tr("表") : tr("裏"))
								   .arg(p->pos.x())
								   .arg(p->pos.y())
								   .arg(static_cast<int>(p->rot));
		if (!filter.isEmpty() && !p->refDes.contains(filter, Qt::CaseInsensitive) &&
			!p->value.contains(filter, Qt::CaseInsensitive) && !partName.contains(filter, Qt::CaseInsensitive)) {
			continue;
		}
		auto *item = new QTreeWidgetItem(placementRoot);
		item->setText(1, label);
		item->setData(1, kKindRole, KindPlacement);
		item->setData(1, kUuidRole, p->uuid);
		item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
		item->setCheckState(0, p->visible ? Qt::Checked : Qt::Unchecked);
	}

	auto *wireRoot = new QTreeWidgetItem(m_tree);
	wireRoot->setText(1, tr("配線 (%1)").arg(doc->wires.size()));
	wireRoot->setData(1, kKindRole, KindWireRoot);
	wireRoot->setFlags(wireRoot->flags() & ~Qt::ItemIsUserCheckable);

	for (const auto &w : doc->wires) {
		const QString layerName = wireLayerDisplayName(w->layer);
		if (!filter.isEmpty() && !layerName.contains(filter, Qt::CaseInsensitive)) {
			continue;
		}
		auto *item = new QTreeWidgetItem(wireRoot);
		item->setText(1, tr("%1  %2点").arg(layerName).arg(w->points.size()));
		item->setData(1, kKindRole, KindWire);
		item->setData(1, kUuidRole, w->uuid);
		item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
		item->setCheckState(0, w->visible ? Qt::Checked : Qt::Unchecked);
	}

	m_tree->expandAll();
	m_rebuilding = false;
	syncTreeSelectionFromScenes();
}

void ObjectListPanel::onFilterTextChanged(const QString &) {
	rebuildTree();
}

void ObjectListPanel::onItemChanged(QTreeWidgetItem *item, int column) {
	if (m_rebuilding || column != 0 || !m_ctx || !m_ctx->document) {
		return;
	}
	const int kind = item->data(1, kKindRole).toInt();
	const QString uuid = item->data(1, kUuidRole).toString();
	const bool newVisible = item->checkState(0) == Qt::Checked;
	auto *stack = m_ctx->document->undoStack();
	if (kind == KindPlacement) {
		const int idx = m_ctx->document->indexOfPlacement(uuid);
		if (idx >= 0 && m_ctx->document->placements[idx]->visible != newVisible) {
			stack->push(new SetPlacementVisibleCommand(m_ctx, uuid, m_ctx->document->placements[idx]->visible,
													   newVisible));
		}
	} else if (kind == KindWire) {
		const int idx = m_ctx->document->indexOfWire(uuid);
		if (idx >= 0 && m_ctx->document->wires[idx]->visible != newVisible) {
			stack->push(new SetWireVisibleCommand(m_ctx, uuid, m_ctx->document->wires[idx]->visible, newVisible));
		}
	}
}

void ObjectListPanel::onCurrentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *) {
	if (m_rebuilding || m_syncingFromScene || !m_ctx) {
		return;
	}
	QSet<QString> placementUuids;
	QSet<QString> wireUuids;
	for (QTreeWidgetItem *item : m_tree->selectedItems()) {
		const int kind = item->data(1, kKindRole).toInt();
		const QString uuid = item->data(1, kUuidRole).toString();
		if (kind == KindPlacement) placementUuids.insert(uuid);
		if (kind == KindWire) wireUuids.insert(uuid);
	}
	m_syncingFromTree = true;
	for (BoardScene *scene : {m_ctx->frontScene, m_ctx->backScene}) {
		if (!scene) continue;
		for (auto *pi : scene->allPlacementItems()) pi->setSelected(placementUuids.contains(pi->placement()->uuid));
		for (auto *wi : scene->allWireItems()) wi->setSelected(wireUuids.contains(wi->wire()->uuid));
	}
	m_syncingFromTree = false;
}

void ObjectListPanel::onItemDoubleClicked(QTreeWidgetItem *item, int) {
	if (!item || !m_ctx) {
		return;
	}
	const int kind = item->data(1, kKindRole).toInt();
	const QString uuid = item->data(1, kUuidRole).toString();
	if (kind == KindPlacement) {
		jumpTo(uuid, QString());
	} else if (kind == KindWire) {
		jumpTo(QString(), uuid);
	}
}

void ObjectListPanel::jumpTo(const QString &placementUuid, const QString &wireUuid) {
	if (!m_ctx || !m_ctx->document) {
		return;
	}
	QPointF target;
	bool found = false;
	if (!placementUuid.isEmpty()) {
		const int idx = m_ctx->document->indexOfPlacement(placementUuid);
		if (idx >= 0) {
			target = QPointF(m_ctx->document->placements[idx]->pos);
			found = true;
		}
	} else if (!wireUuid.isEmpty()) {
		const int idx = m_ctx->document->indexOfWire(wireUuid);
		if (idx >= 0 && !m_ctx->document->wires[idx]->points.isEmpty()) {
			target = QPointF(m_ctx->document->wires[idx]->points.first());
			found = true;
		}
	}
	if (!found) {
		return;
	}
	for (BoardScene *scene : {m_ctx->frontScene, m_ctx->backScene}) {
		if (!scene) continue;
		auto *view = qobject_cast<BoardView *>(scene->views().isEmpty() ? nullptr : scene->views().first());
		if (view) view->centerOnModel(target);
	}
}

void ObjectListPanel::onContextMenuRequested(const QPoint &pos) {
	QTreeWidgetItem *item = m_tree->itemAt(pos);
	if (!item || !m_ctx || !m_ctx->document) {
		return;
	}
	const int kind = item->data(1, kKindRole).toInt();
	if (kind != KindPlacement && kind != KindWire) {
		return;
	}
	const QString uuid = item->data(1, kUuidRole).toString();
	auto *stack = m_ctx->document->undoStack();

	QMenu menu;
	QAction *toggleVisible = menu.addAction(item->checkState(0) == Qt::Checked ? tr("非表示にする") : tr("表示する"));
	QAction *rotateAction = kind == KindPlacement ? menu.addAction(tr("回転 (+90°)")) : nullptr;
	QAction *flipAction = kind == KindPlacement ? menu.addAction(tr("表裏切替")) : nullptr;
	QAction *deleteAction = menu.addAction(tr("削除"));
	QAction *chosen = menu.exec(m_tree->viewport()->mapToGlobal(pos));
	if (!chosen) {
		return;
	}

	if (chosen == toggleVisible) {
		item->setCheckState(0, item->checkState(0) == Qt::Checked ? Qt::Unchecked : Qt::Checked);
	} else if (kind == KindPlacement && chosen == rotateAction) {
		const int idx = m_ctx->document->indexOfPlacement(uuid);
		if (idx >= 0) {
			const Rotation oldRot = m_ctx->document->placements[idx]->rot;
			stack->push(new RotatePlacementCommand(m_ctx, uuid, oldRot, rotateCW(oldRot)));
		}
	} else if (kind == KindPlacement && chosen == flipAction) {
		const int idx = m_ctx->document->indexOfPlacement(uuid);
		if (idx >= 0) {
			const Side oldSide = m_ctx->document->placements[idx]->side;
			stack->push(
				new FlipPlacementSideCommand(m_ctx, uuid, oldSide, oldSide == Side::Front ? Side::Back : Side::Front));
		}
	} else if (chosen == deleteAction) {
		if (kind == KindPlacement) {
			stack->push(new RemovePlacementCommand(m_ctx, uuid));
		} else {
			stack->push(new RemoveWireCommand(m_ctx, uuid));
		}
	}
}

void ObjectListPanel::onFrontSelectionChanged() {
	if (m_syncingFromTree) return;
	syncTreeSelectionFromScenes();
}

void ObjectListPanel::onBackSelectionChanged() {
	if (m_syncingFromTree) return;
	syncTreeSelectionFromScenes();
}

void ObjectListPanel::syncTreeSelectionFromScenes() {
	if (!m_ctx || !m_ctx->frontScene) {
		return;
	}
	QSet<QString> placementUuids;
	QSet<QString> wireUuids;
	for (auto *pi : m_ctx->frontScene->allPlacementItems()) {
		if (pi->isSelected()) placementUuids.insert(pi->placement()->uuid);
	}
	for (auto *wi : m_ctx->frontScene->allWireItems()) {
		if (wi->isSelected()) wireUuids.insert(wi->wire()->uuid);
	}
	m_syncingFromScene = true;
	QTreeWidgetItemIterator it(m_tree);
	while (*it) {
		const int kind = (*it)->data(1, kKindRole).toInt();
		const QString uuid = (*it)->data(1, kUuidRole).toString();
		const bool shouldSelect = (kind == KindPlacement && placementUuids.contains(uuid)) ||
								  (kind == KindWire && wireUuids.contains(uuid));
		(*it)->setSelected(shouldSelect);
		++it;
	}
	m_syncingFromScene = false;
}
