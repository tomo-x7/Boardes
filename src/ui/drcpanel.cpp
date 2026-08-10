#include "drcpanel.h"

#include <QAbstractItemView>
#include <QBrush>
#include <QColor>
#include <QHeaderView>
#include <QLabel>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "theme.h"

namespace {

QString severityText(DrcSeverity s) {
	return s == DrcSeverity::Error ? QStringLiteral("エラー") : QStringLiteral("警告");
}

// 仕様書§3.2のセマンティックトークン (error/warning)。ライト/ダークで値が異なるため
// Theme 経由で取得する。
QColor severityColor(DrcSeverity s) {
	return s == DrcSeverity::Error ? Theme::instance().errorColor() : Theme::instance().warningColor();
}

}  // namespace

DrcPanel::DrcPanel(QWidget *parent) : QWidget(parent) {
	m_summaryLabel = new QLabel(QStringLiteral("(ドキュメント未設定)"), this);

	m_tree = new QTreeWidget(this);
	m_tree->setColumnCount(2);
	m_tree->setHeaderLabels({QStringLiteral("重要度"), QStringLiteral("内容")});
	m_tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	m_tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
	m_tree->setRootIsDecorated(false);
	m_tree->setUniformRowHeights(true);
	m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
	m_tree->setAlternatingRowColors(true);

	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(4, 4, 4, 4);
	layout->addWidget(m_summaryLabel);
	layout->addWidget(m_tree, /*stretch=*/1);

	connect(m_tree, &QTreeWidget::itemClicked, this, &DrcPanel::onItemClicked);
	// OS側のライト/ダーク切替で重要度の文字色を塗り直す。
	connect(&Theme::instance(), &Theme::changed, this, &DrcPanel::refresh);
}

void DrcPanel::setContext(Document *document, LibraryManager *libraryManager) {
	m_document = document;
	m_libraryManager = libraryManager;
	refresh();
}

void DrcPanel::refresh() {
	m_tree->clear();
	m_findings.clear();

	if (!m_document || !m_libraryManager) {
		m_summaryLabel->setText(QStringLiteral("(ドキュメント未設定)"));
		return;
	}

	// 基板規模的に毎回全件走査で十分高速なので、増分計算はせずまるごと再計算する。
	m_findings = m_engine.run(*m_document, m_libraryManager);

	int errorCount = 0;
	int warningCount = 0;
	for (int i = 0; i < m_findings.size(); ++i) {
		const DrcFinding &f = m_findings[i];
		if (f.severity == DrcSeverity::Error) {
			++errorCount;
		} else {
			++warningCount;
		}
		auto *item = new QTreeWidgetItem(m_tree);
		item->setText(0, severityText(f.severity));
		item->setForeground(0, QBrush(severityColor(f.severity)));
		item->setText(1, f.message);
		item->setData(0, Qt::UserRole, i);  // m_findings 内のインデックスを持たせておく (コンストラクタで m_tree に自動追加済み)
	}

	if (m_findings.isEmpty()) {
		m_summaryLabel->setText(QStringLiteral("問題は見つかりませんでした。"));
	} else {
		m_summaryLabel->setText(QStringLiteral("エラー %1件 / 警告 %2件").arg(errorCount).arg(warningCount));
	}
}

void DrcPanel::onItemClicked(QTreeWidgetItem *item, int /*column*/) {
	if (!item) {
		return;
	}
	const int idx = item->data(0, Qt::UserRole).toInt();
	if (idx < 0 || idx >= m_findings.size()) {
		return;
	}
	emit findingActivated(m_findings[idx]);
}
