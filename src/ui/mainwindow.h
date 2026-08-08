#pragma once

#include <QHash>
#include <QMainWindow>
#include <memory>
#include <optional>

#include "../io/imageexport.h"
#include "../io/loadresult.h"
#include "../model/document.h"
#include "../model/librarymanager.h"
#include "../model/wire.h"
#include "actionregistry.h"
#include "input/keymap.h"
#include "tools/snapengine.h"
#include "tools/toolmanager.h"

namespace Ui {
class MainWindow;
}

class BoardScene;
class BoardView;
class PartSelector;
class DrcPanel;
class StatsPanel;
class ObjectListPanel;
struct DrcFinding;
class QSplitter;
class QActionGroup;
class QMenu;
class QDockWidget;
class QToolBar;
class ZoomBar;
class ViewLinkController;

class MainWindow : public QMainWindow {
	Q_OBJECT

public:
	explicit MainWindow(QWidget *parent = nullptr);
	~MainWindow();

	// スクリーンショット採取ハーネス (design/screenshot_all.cpp) が、実際に構築された
	// ツールバーと同じ ActionRegistry を使って ToolbarCustomizeDialog を単独表示するために
	// 公開している。通常の操作では MainWindow 自身が内部でしか使わない。
	ActionRegistry &actionRegistry() {
		return m_actionRegistry;
	}

protected:
	void closeEvent(QCloseEvent *event) override;

private slots:
	void onAboutTriggered();
	void onManageLibrariesTriggered();
	void onPartSelected(const QString &libraryId, const QString &partId);
	void onToggleViewOrientation();

	void onNewTriggered();
	void onOpenTriggered();
	bool onSaveTriggered();
	bool onSaveAsTriggered();
	void onOpenRecentFile(const QString &path);
	void onUndoStackCleanChanged(bool clean);

	void onExportPackageTriggered();
	void onExportPngTriggered();
	void onExportSvgTriggered();
	void onExportClipboardTriggered();

	void onDrcFindingActivated(const DrcFinding &finding);
	void onEscapeTriggered();

private:
	Ui::MainWindow *ui;

	LibraryManager m_libraryManager;
	std::unique_ptr<Document> m_document;
	QString m_currentFilePath;  // 空 = 無題 (未保存)
	SnapEngine m_snapEngine;
	Keymap m_keymap;  // ショートカット/マウス割り当てのカスタマイズ (Phase 18)
	std::unique_ptr<ToolManager> m_toolManager;

	BoardScene *m_frontScene;
	BoardScene *m_backScene;
	BoardView *m_frontView;
	BoardView *m_backView;
	PartSelector *m_partSelector;
	QSplitter *m_viewsSplitter;
	QActionGroup *m_toolActionGroup = nullptr;
	QMenu *m_recentFilesMenu = nullptr;
	DrcPanel *m_drcPanel = nullptr;
	StatsPanel *m_statsPanel = nullptr;
	ObjectListPanel *m_objectListPanel = nullptr;
	ZoomBar *m_zoomBar = nullptr;
	ViewLinkController *m_viewLink = nullptr;

	// ツールバーのカスタマイズ (Phase 19)。commandId → QAction の登録簿と、
	// id → 実際の QToolBar。「表示」「ツール」は builtin (設立済み)、それ以外は
	// ユーザーが「新しいツールバー」で作成したもの。
	ActionRegistry m_actionRegistry;
	QHash<QString, QToolBar *> m_toolbars;

	void setupCentralWidget();
	void setupViewMenuAndToolbar();
	void setupToolsToolbar();
	void setupFileMenuConnections();
	void setupSidePanelDock();
	// toolbarlayout::load() の内容を実際のツールバーへ反映する。builtin (表示/ツール)
	// は表示/非表示とボタンスタイルのみ変更し、それ以外 (ユーザー作成分) は
	// 毎回組み立て直す。ToolbarCustomizeDialog::layoutsChanged からも呼ばれる。
	void rebuildToolbars();
	void setDocument(std::unique_ptr<Document> doc, const QString &filePath = QString());
	void syncAllScenes();
	// 基板を Undo コマンド経由で設定し、直後に両ビューを全体表示にする
	// (ウィジェットのサイズ確定を待つため 0ms 遅延)。
	void pushBoardAndFit(const BoardSpec &board);
	void fitViewsDeferred();

	// true を返したら「処理を続けて良い」(保存した/破棄を選んだ/そもそも未変更)。
	// false は利用者がキャンセルしたことを意味する。
	bool maybeSaveChanges();
	void updateWindowTitle();
	QString displayTitle() const;

	QStringList recentFiles() const;
	void addToRecentFiles(const QString &path);
	void rebuildRecentFilesMenu();

	// 読み込みに失敗したファイルについて、理由付きのダイアログを出す
	// (「ファイルを削除」「詳細をコピー」「閉じる」の3択、既定は閉じる)。Phase 17。
	void showCorruptedFileDialog(const QString &path, const LoadResult &result);
	// 起動直後、LibraryManager::loadIssues() が空でなければまとめて警告する。
	void checkLibraryLoadIssues();

	std::optional<imageexport::Options> promptExportOptions();
};
