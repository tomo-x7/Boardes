#include "statspanel.h"

#include <QAbstractItemView>
#include <QFileDialog>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include "../model/document.h"

namespace {

// View メニュー/ツールバーのレイヤ表示トグルと同じ日本語名に揃える。
QString wireLayerDisplayName(WireLayer layer) {
	switch (layer) {
	case WireLayer::FrontBare:
		return QStringLiteral("表面配線");
	case WireLayer::BackBare:
		return QStringLiteral("裏面配線");
	case WireLayer::FrontInsulated:
		return QStringLiteral("表面被覆配線");
	case WireLayer::BackInsulated:
		return QStringLiteral("裏面被覆配線");
	case WireLayer::Outline:
		return QStringLiteral("外形線");
	}
	return QStringLiteral("配線");
}

// 表示順を固定するための一覧 (QHash のイテレーション順に依存しないようにする)。
constexpr WireLayer kLayerDisplayOrder[] = {WireLayer::FrontBare, WireLayer::BackBare, WireLayer::FrontInsulated,
											 WireLayer::BackInsulated, WireLayer::Outline};

}  // namespace

StatsPanel::StatsPanel(QWidget *parent) : QWidget(parent) {
	m_summaryLabel = new QLabel(QStringLiteral("(ドキュメント未設定)"), this);
	m_summaryLabel->setTextFormat(Qt::PlainText);
	m_summaryLabel->setWordWrap(true);
	m_summaryLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);

	m_bomTable = new QTableWidget(this);
	m_bomTable->setColumnCount(5);
	m_bomTable->setHorizontalHeaderLabels(
		{QStringLiteral("refDes"), QStringLiteral("部品名"), QStringLiteral("値"), QStringLiteral("ライブラリ"),
		 QStringLiteral("数量")});
	m_bomTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
	m_bomTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
	m_bomTable->verticalHeader()->setVisible(false);
	m_bomTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_bomTable->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_bomTable->setAlternatingRowColors(true);

	m_exportButton = new QPushButton(QStringLiteral("BOM を CSV でエクスポート..."), this);
	connect(m_exportButton, &QPushButton::clicked, this, &StatsPanel::onExportClicked);

	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(4, 4, 4, 4);
	layout->addWidget(m_summaryLabel);
	layout->addWidget(new QLabel(QStringLiteral("BOM (部品表)"), this));
	layout->addWidget(m_bomTable, /*stretch=*/1);
	layout->addWidget(m_exportButton);
}

void StatsPanel::setContext(Document *document, LibraryManager *libraryManager) {
	m_document = document;
	m_libraryManager = libraryManager;
	refresh();
}

void StatsPanel::refresh() {
	m_bomTable->setRowCount(0);
	m_bom.clear();

	if (!m_document || !m_libraryManager) {
		m_summaryLabel->setText(QStringLiteral("(ドキュメント未設定)"));
		return;
	}

	const BoardStats stats = m_engine.compute(*m_document, m_libraryManager);

	QStringList lines;
	QStringList wireLines;
	for (WireLayer layer : kLayerDisplayOrder) {
		const double mm = stats.wireLengthMmByLayer.value(layer, 0.0);
		if (mm > 0.0) {
			wireLines << QStringLiteral("%1 %2mm").arg(wireLayerDisplayName(layer)).arg(mm, 0, 'f', 2);
		}
	}
	lines << QStringLiteral("配線長合計: %1mm%2")
				 .arg(stats.totalWireLengthMm, 0, 'f', 2)
				 .arg(wireLines.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(wireLines.join(QStringLiteral(" / "))));
	lines << QStringLiteral("穴数合計: %1 (部品ピン %2 / 表面裸線端点 %3 / スルーホール %4)")
				 .arg(stats.totalHoleCount)
				 .arg(stats.componentPinHoleCount)
				 .arg(stats.frontBareWireEndpointCount)
				 .arg(stats.thruHoleCount);
	lines << QStringLiteral("基板面積: %1mm² / 部品占有面積: %2mm² (占有率 %3%)")
				 .arg(stats.boardAreaMm2, 0, 'f', 1)
				 .arg(stats.occupiedAreaMm2, 0, 'f', 1)
				 .arg(stats.occupancyRatio * 100.0, 0, 'f', 1);
	lines << QStringLiteral("部品点数: %1 / 配線本数: %2").arg(stats.placementCount).arg(stats.wireCount);
	m_summaryLabel->setText(lines.join(QStringLiteral("\n")));

	m_bom = m_engine.computeBom(*m_document, m_libraryManager);
	m_bomTable->setRowCount(m_bom.size());
	for (int row = 0; row < m_bom.size(); ++row) {
		const BomRow &r = m_bom[row];
		m_bomTable->setItem(row, 0, new QTableWidgetItem(r.refDesList));
		m_bomTable->setItem(row, 1, new QTableWidgetItem(r.partName));
		m_bomTable->setItem(row, 2, new QTableWidgetItem(r.value));
		m_bomTable->setItem(row, 3, new QTableWidgetItem(r.libraryName));
		auto *qtyItem = new QTableWidgetItem(QString::number(r.quantity));
		qtyItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
		m_bomTable->setItem(row, 4, qtyItem);
	}
}

void StatsPanel::onExportClicked() {
	if (m_bom.isEmpty()) {
		QMessageBox::information(this, QStringLiteral("BOM エクスポート"), QStringLiteral("部品が配置されていません。"));
		return;
	}
	QString path =
		QFileDialog::getSaveFileName(this, QStringLiteral("BOM を CSV でエクスポート"), QString(), QStringLiteral("CSV (*.csv)"));
	if (path.isEmpty()) {
		return;
	}
	if (!path.endsWith(QStringLiteral(".csv"), Qt::CaseInsensitive)) {
		path += QStringLiteral(".csv");
	}
	if (!StatsEngine::saveBomCsv(m_bom, path)) {
		QMessageBox::warning(this, QStringLiteral("エクスポート失敗"), QStringLiteral("CSV の書き出しに失敗しました。"));
	}
}
