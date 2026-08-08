#pragma once

#include <QWidget>

#include "../model/drc.h"

class Document;
class LibraryManager;
class QTreeWidget;
class QTreeWidgetItem;
class QLabel;

// DRC (デザインルールチェック) の結果一覧パネル。
//
// 編集のたびに (基板規模的に全件走査で十分高速なため、増分ではなく) 毎回まとめて
// 再検査する (MainWindow が QUndoStack::indexChanged / LibraryManager::librariesChanged
// の両方から refresh() を呼ぶ)。手動の「再検査」ボタンは無い — 完全なリアルタイムで
// ないなら別の箇所 (再検査が必要なタイミングの検出漏れ) を直すべき、というのが
// Phase 15 での判断。行をクリックすると該当箇所へジャンプしてほしい旨を
// findingActivated シグナルで外部 (MainWindow) に伝える — ジャンプ・選択の実際の
// 処理はビュー/シーンを持つ MainWindow 側の責務とする。
class DrcPanel : public QWidget {
	Q_OBJECT

public:
	explicit DrcPanel(QWidget *parent = nullptr);

	void setContext(Document *document, LibraryManager *libraryManager);

public slots:
	// ドキュメントの内容が変わるたびに呼ぶ (undo/redo/編集コマンドいずれでも)。
	void refresh();

signals:
	void findingActivated(const DrcFinding &finding);

private:
	Document *m_document = nullptr;
	LibraryManager *m_libraryManager = nullptr;
	DrcEngine m_engine;
	QVector<DrcFinding> m_findings;

	QLabel *m_summaryLabel;
	QTreeWidget *m_tree;

	void onItemClicked(QTreeWidgetItem *item, int column);
};
