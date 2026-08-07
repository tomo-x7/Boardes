#pragma once

#include <QWidget>

#include "../model/stats.h"

class Document;
class LibraryManager;
class QLabel;
class QTableWidget;
class QPushButton;

// 配線長・穴数・占有面積などの統計と、BOM (部品表) を表示するパネル。
// DrcPanel と同様、編集のたびに (基板規模的に全件走査で十分高速なため)
// 増分計算はせず毎回まるごと再計算する。
class StatsPanel : public QWidget {
	Q_OBJECT

public:
	explicit StatsPanel(QWidget *parent = nullptr);

	void setContext(Document *document, LibraryManager *libraryManager);

public slots:
	void refresh();

private:
	Document *m_document = nullptr;
	LibraryManager *m_libraryManager = nullptr;
	StatsEngine m_engine;
	QVector<BomRow> m_bom;

	QLabel *m_summaryLabel;
	QTableWidget *m_bomTable;
	QPushButton *m_exportButton;

	void onExportClicked();
};
