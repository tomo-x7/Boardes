#pragma once

#include <QMainWindow>
#include <memory>
#include <optional>

#include "../io/imageexport.h"
#include "../model/document.h"
#include "../model/librarymanager.h"
#include "../model/wire.h"
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
struct DrcFinding;
class QSplitter;
class QActionGroup;
class QMenu;
class QDockWidget;

class MainWindow : public QMainWindow {
	Q_OBJECT

public:
	explicit MainWindow(QWidget *parent = nullptr);
	~MainWindow();

protected:
	void closeEvent(QCloseEvent *event) override;

private slots:
	void onAboutTriggered();
	void onImportPassFolderTriggered();
	void onImportPartFileTriggered();
	void onImportBoardFileTriggered();
	void onCreateBoardTriggered();
	void onCreatePartTriggered();
	void onManageLibrariesTriggered();
	void onPartActivated(const QString &libraryId, const QString &partId);
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

private:
	Ui::MainWindow *ui;

	LibraryManager m_libraryManager;
	std::unique_ptr<Document> m_document;
	QString m_currentFilePath;  // 空 = 無題 (未保存)
	SnapEngine m_snapEngine;
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

	void setupCentralWidget();
	void setupViewMenuAndToolbar();
	void setupToolsToolbar();
	void setupFileMenuConnections();
	void setupAnalysisDock();
	void setDocument(std::unique_ptr<Document> doc, const QString &filePath = QString());
	void syncAllScenes();
	void pickDefaultBoardIfNeeded(const QString &libraryId);

	// true を返したら「処理を続けて良い」(保存した/破棄を選んだ/そもそも未変更)。
	// false は利用者がキャンセルしたことを意味する。
	bool maybeSaveChanges();
	void updateWindowTitle();
	QString displayTitle() const;

	QStringList recentFiles() const;
	void addToRecentFiles(const QString &path);
	void rebuildRecentFilesMenu();

	std::optional<imageexport::Options> promptExportOptions();
};
