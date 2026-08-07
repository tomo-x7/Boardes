#include "mainwindow.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QSettings>
#include <QSplitter>
#include <QTabWidget>
#include <QToolBar>
#include <QVBoxLayout>
#include <functional>

#include "../commands/boardcommands.h"
#include "../core/units.h"
#include "../io/documentio.h"
#include "../model/drc.h"
#include "../render/boardscene.h"
#include "../render/boardview.h"
#include "../render/items/placementitem.h"
#include "../render/items/wireitem.h"
#include "about.h"
#include "boardeditordialog.h"
#include "drcpanel.h"
#include "exportimageoptionsdialog.h"
#include "librarymanagerdialog.h"
#include "parteditordialog.h"
#include "partselector.h"
#include "statspanel.h"
#include "ui_mainwindow.h"

namespace {
constexpr int kMaxRecentFiles = 10;
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow) {
	ui->setupUi(this);

	connect(ui->aboutAction, &QAction::triggered, this, &MainWindow::onAboutTriggered);
	connect(ui->importPassAction, &QAction::triggered, this, &MainWindow::onImportPassFolderTriggered);
	connect(ui->importPartFileAction, &QAction::triggered, this, &MainWindow::onImportPartFileTriggered);
	connect(ui->importBoardFileAction, &QAction::triggered, this, &MainWindow::onImportBoardFileTriggered);
	connect(ui->createBoardAction, &QAction::triggered, this, &MainWindow::onCreateBoardTriggered);
	connect(ui->createPartAction, &QAction::triggered, this, &MainWindow::onCreatePartTriggered);
	connect(ui->manageLibrariesAction, &QAction::triggered, this, &MainWindow::onManageLibrariesTriggered);
	setupFileMenuConnections();

	const QSettings settings;
	if (settings.contains("geometry")) {
		restoreGeometry(settings.value("geometry").toByteArray());
	}
	if (settings.contains("windowState")) {
		restoreState(settings.value("windowState").toByteArray());
	}

	m_libraryManager.loadAll();

	setupCentralWidget();
	setupAnalysisDock();
	setDocument(std::make_unique<Document>());

	ToolContext ctx;
	ctx.document = m_document.get();
	ctx.libraryManager = &m_libraryManager;
	ctx.frontScene = m_frontScene;
	ctx.backScene = m_backScene;
	ctx.snapEngine = &m_snapEngine;
	m_toolManager = std::make_unique<ToolManager>(ctx, this);
	m_frontScene->setToolManager(m_toolManager.get());
	m_backScene->setToolManager(m_toolManager.get());
	connect(m_toolManager.get(), &ToolManager::statusHintChanged, this,
			[this](const QString &hint) { ui->statusbar->showMessage(hint); });

	setupViewMenuAndToolbar();
	setupToolsToolbar();
	rebuildRecentFilesMenu();
	updateWindowTitle();

	m_partSelector->setLibraryManager(&m_libraryManager);
	connect(m_partSelector, &PartSelector::partActivated, this, &MainWindow::onPartActivated);
}

MainWindow::~MainWindow() {
	delete ui;
}

void MainWindow::setupCentralWidget() {
	m_partSelector = new PartSelector(this);

	auto makeViewBox = [this](const QString &title, BoardView *&view) -> QWidget * {
		auto *box = new QWidget();
		auto *layout = new QVBoxLayout(box);
		layout->setContentsMargins(0, 0, 0, 0);
		layout->setSpacing(0);
		auto *label = new QLabel(title, box);
		label->setAlignment(Qt::AlignCenter);
		label->setStyleSheet(QStringLiteral("QLabel { background: palette(mid); font-weight: bold; padding: 2px; }"));
		view = new BoardView(box);
		layout->addWidget(label);
		layout->addWidget(view, 1);
		return box;
	};

	QWidget *frontBox = makeViewBox(QStringLiteral("表面"), m_frontView);
	QWidget *backBox = makeViewBox(QStringLiteral("裏面"), m_backView);

	m_viewsSplitter = new QSplitter(Qt::Horizontal);
	m_viewsSplitter->addWidget(frontBox);
	m_viewsSplitter->addWidget(backBox);

	auto *mainSplitter = new QSplitter(Qt::Horizontal, ui->centralwidget);
	mainSplitter->addWidget(m_partSelector);
	mainSplitter->addWidget(m_viewsSplitter);
	mainSplitter->setStretchFactor(0, 0);
	mainSplitter->setStretchFactor(1, 1);
	mainSplitter->setSizes({250, 1000});

	auto *layout = new QHBoxLayout(ui->centralwidget);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(mainSplitter);

	m_frontScene = new BoardScene(Side::Front, this);
	m_backScene = new BoardScene(Side::Back, this);
	m_frontView->setScene(m_frontScene);
	m_backView->setScene(m_backScene);
}

void MainWindow::setupAnalysisDock() {
	auto *dock = new QDockWidget(QStringLiteral("解析"), this);
	dock->setObjectName(QStringLiteral("analysisDock"));

	auto *tabs = new QTabWidget(dock);
	m_drcPanel = new DrcPanel(tabs);
	m_statsPanel = new StatsPanel(tabs);
	tabs->addTab(m_drcPanel, QStringLiteral("DRC"));
	tabs->addTab(m_statsPanel, QStringLiteral("統計"));
	dock->setWidget(tabs);
	addDockWidget(Qt::RightDockWidgetArea, dock);
	ui->menuView->addAction(dock->toggleViewAction());

	connect(m_drcPanel, &DrcPanel::findingActivated, this, &MainWindow::onDrcFindingActivated);
}

void MainWindow::setupViewMenuAndToolbar() {
	auto *toolbar = addToolBar(QStringLiteral("表示"));
	toolbar->setObjectName(QStringLiteral("viewToolBar"));

	QAction *zoomInAction = toolbar->addAction(QStringLiteral("拡大"));
	connect(zoomInAction, &QAction::triggered, this, [this] {
		m_frontView->zoomIn();
		m_backView->zoomIn();
	});
	QAction *zoomOutAction = toolbar->addAction(QStringLiteral("縮小"));
	connect(zoomOutAction, &QAction::triggered, this, [this] {
		m_frontView->zoomOut();
		m_backView->zoomOut();
	});
	QAction *zoomResetAction = toolbar->addAction(QStringLiteral("100%"));
	connect(zoomResetAction, &QAction::triggered, this, [this] {
		m_frontView->resetZoom();
		m_backView->resetZoom();
	});
	QAction *fitAction = toolbar->addAction(QStringLiteral("全体表示"));
	fitAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
	connect(fitAction, &QAction::triggered, this, [this] {
		m_frontView->fitBoardToWindow();
		m_backView->fitBoardToWindow();
	});

	toolbar->addSeparator();

	QAction *orientationAction = toolbar->addAction(QStringLiteral("表裏レイアウト切替"));
	connect(orientationAction, &QAction::triggered, this, &MainWindow::onToggleViewOrientation);

	toolbar->addSeparator();

	auto addToggle = [&](const QString &text, bool checked, const std::function<void(bool)> &onToggled) {
		QAction *a = toolbar->addAction(text);
		a->setCheckable(true);
		a->setChecked(checked);
		connect(a, &QAction::toggled, this, onToggled);
		ui->menuView->addAction(a);
	};

	addToggle(QStringLiteral("表面配線"), true, [this](bool v) {
		m_frontScene->setLayerVisible(WireLayer::FrontBare, v);
		m_backScene->setLayerVisible(WireLayer::FrontBare, v);
	});
	addToggle(QStringLiteral("裏面配線"), true, [this](bool v) {
		m_frontScene->setLayerVisible(WireLayer::BackBare, v);
		m_backScene->setLayerVisible(WireLayer::BackBare, v);
	});
	addToggle(QStringLiteral("表面被覆配線"), true, [this](bool v) {
		m_frontScene->setLayerVisible(WireLayer::FrontInsulated, v);
		m_backScene->setLayerVisible(WireLayer::FrontInsulated, v);
	});
	addToggle(QStringLiteral("裏面被覆配線"), true, [this](bool v) {
		m_frontScene->setLayerVisible(WireLayer::BackInsulated, v);
		m_backScene->setLayerVisible(WireLayer::BackInsulated, v);
	});
	addToggle(QStringLiteral("外形線"), true, [this](bool v) {
		m_frontScene->setLayerVisible(WireLayer::Outline, v);
		m_backScene->setLayerVisible(WireLayer::Outline, v);
	});
	addToggle(QStringLiteral("部品アウトライン表示"), false, [this](bool v) {
		m_frontScene->setForcePartOutline(v);
		m_backScene->setForcePartOutline(v);
	});
	addToggle(QStringLiteral("ピン番号表示"), false, [this](bool v) {
		m_frontScene->setShowPinNumbers(v);
		m_backScene->setShowPinNumbers(v);
	});
	addToggle(QStringLiteral("部品番号/値表示"), true, [this](bool v) {
		m_frontScene->setShowLabels(v);
		m_backScene->setShowLabels(v);
	});
}

void MainWindow::setupToolsToolbar() {
	auto *toolbar = addToolBar(QStringLiteral("ツール"));
	toolbar->setObjectName(QStringLiteral("toolsToolBar"));
	insertToolBarBreak(toolbar);  // 表示ツールバーとは別行に

	m_toolActionGroup = new QActionGroup(this);
	m_toolActionGroup->setExclusive(true);

	auto addToolAction = [&](const QString &text, const std::function<void()> &activate) -> QAction * {
		QAction *a = toolbar->addAction(text);
		a->setCheckable(true);
		m_toolActionGroup->addAction(a);
		connect(a, &QAction::triggered, this, [activate] { activate(); });
		return a;
	};

	QAction *selectAction = addToolAction(QStringLiteral("選択"), [this] { m_toolManager->activateSelectTool(); });
	selectAction->setChecked(true);
	toolbar->addSeparator();
	addToolAction(QStringLiteral("配線(表面)"), [this] { m_toolManager->activateWireTool(WireLayer::FrontBare); });
	addToolAction(QStringLiteral("配線(裏面)"), [this] { m_toolManager->activateWireTool(WireLayer::BackBare); });
	addToolAction(QStringLiteral("被覆配線(表面)"),
				 [this] { m_toolManager->activateWireTool(WireLayer::FrontInsulated); });
	addToolAction(QStringLiteral("被覆配線(裏面)"),
				 [this] { m_toolManager->activateWireTool(WireLayer::BackInsulated); });
	toolbar->addSeparator();
	addToolAction(QStringLiteral("下書き"), [this] { m_toolManager->activateDraftTool(); });

	toolbar->addSeparator();
	toolbar->addWidget(new QLabel(QStringLiteral(" 配線スナップ: ")));
	auto *snapCombo = new QComboBox();
	snapCombo->addItem(QStringLiteral("フル (2.54mm)"), static_cast<int>(units::Granularity::Full));
	snapCombo->addItem(QStringLiteral("ハーフ (1.27mm)"), static_cast<int>(units::Granularity::Half));
	snapCombo->addItem(QStringLiteral("フリー"), static_cast<int>(units::Granularity::Free));
	connect(snapCombo, &QComboBox::currentIndexChanged, this, [this, snapCombo](int) {
		m_snapEngine.setGranularity(static_cast<units::Granularity>(snapCombo->currentData().toInt()));
	});
	toolbar->addWidget(snapCombo);
}

void MainWindow::setupFileMenuConnections() {
	connect(ui->newAction, &QAction::triggered, this, &MainWindow::onNewTriggered);
	connect(ui->openAction, &QAction::triggered, this, &MainWindow::onOpenTriggered);
	connect(ui->saveAction, &QAction::triggered, this, [this] { onSaveTriggered(); });
	connect(ui->saveAsAction, &QAction::triggered, this, [this] { onSaveAsTriggered(); });
	connect(ui->exportPackageAction, &QAction::triggered, this, &MainWindow::onExportPackageTriggered);
	connect(ui->exportPngAction, &QAction::triggered, this, &MainWindow::onExportPngTriggered);
	connect(ui->exportSvgAction, &QAction::triggered, this, &MainWindow::onExportSvgTriggered);
	connect(ui->exportClipboardAction, &QAction::triggered, this, &MainWindow::onExportClipboardTriggered);
}

void MainWindow::setDocument(std::unique_ptr<Document> doc, const QString &filePath) {
	m_document = std::move(doc);
	m_currentFilePath = filePath;
	if (m_toolManager) {
		m_toolManager->context().document = m_document.get();
		m_toolManager->activateSelectTool();  // ドキュメント切替時は安全な選択ツールに戻す
	}

	ui->menuEdit->clear();
	QAction *undoAction = m_document->undoStack()->createUndoAction(ui->menuEdit, QStringLiteral("元に戻す"));
	undoAction->setShortcut(QKeySequence::Undo);
	QAction *redoAction = m_document->undoStack()->createRedoAction(ui->menuEdit, QStringLiteral("やり直す"));
	redoAction->setShortcut(QKeySequence::Redo);
	ui->menuEdit->addAction(undoAction);
	ui->menuEdit->addAction(redoAction);

	connect(m_document->undoStack(), &QUndoStack::cleanChanged, this, &MainWindow::onUndoStackCleanChanged);

	if (m_drcPanel) {
		m_drcPanel->setContext(m_document.get(), &m_libraryManager);
		// 基板規模的に全件走査で十分高速なので、push/undo/redo いずれでも毎回まるごと
		// 再検査する (増分計算はしない)。
		connect(m_document->undoStack(), &QUndoStack::indexChanged, m_drcPanel, [this](int) { m_drcPanel->refresh(); });
	}
	if (m_statsPanel) {
		m_statsPanel->setContext(m_document.get(), &m_libraryManager);
		connect(m_document->undoStack(), &QUndoStack::indexChanged, m_statsPanel,
				[this](int) { m_statsPanel->refresh(); });
	}

	syncAllScenes();
	updateWindowTitle();
}

void MainWindow::syncAllScenes() {
	m_frontScene->setDocument(m_document.get(), &m_libraryManager);
	m_backScene->setDocument(m_document.get(), &m_libraryManager);
}

void MainWindow::onAboutTriggered() {
	About about(this);
	about.exec();
}

void MainWindow::onToggleViewOrientation() {
	m_viewsSplitter->setOrientation(m_viewsSplitter->orientation() == Qt::Horizontal ? Qt::Vertical : Qt::Horizontal);
}

void MainWindow::onImportPassFolderTriggered() {
	const QString dir =
		QFileDialog::getExistingDirectory(this, QStringLiteral("PasS の parts フォルダを選択"));
	if (dir.isEmpty()) {
		return;
	}

	const auto existing = m_libraryManager.library(LibraryManager::passCompatId());
	if (existing && !existing->parts.isEmpty()) {
		const auto btn = QMessageBox::question(
			this, QStringLiteral("確認"),
			QStringLiteral("既存の「PasS互換」ライブラリを置き換えます。よろしいですか？"));
		if (btn != QMessageBox::Yes) {
			return;
		}
	}

	const auto result = m_libraryManager.importPassFolder(dir);
	if (!result.ok) {
		QMessageBox::warning(this, QStringLiteral("インポート失敗"), result.error);
		return;
	}

	QString message = QStringLiteral("カテゴリ: %1件 / 部品: %2件 / 基板: %3件を取り込みました。")
						  .arg(result.categoryCount)
						  .arg(result.partCount)
						  .arg(result.boardCount);
	if (!result.issues.isEmpty()) {
		message += QStringLiteral("\n\n読み込めなかったファイル (%1件):\n").arg(result.issues.size());
		message += result.issues.mid(0, 20).join(QStringLiteral("\n"));
		if (result.issues.size() > 20) {
			message += QStringLiteral("\n...");
		}
	}
	QMessageBox::information(this, QStringLiteral("インポート完了"), message);

	pickDefaultBoardIfNeeded(result.libraryId);
}

void MainWindow::pickDefaultBoardIfNeeded(const QString &libraryId) {
	if (!m_document->board.id.isEmpty()) {
		return;  // 既に基板が設定済みならそのままにする
	}
	const auto lib = m_libraryManager.library(libraryId);
	if (!lib || lib->boards.isEmpty()) {
		return;
	}
	auto chosen = lib->boards.value(QStringLiteral("ICB-504"));
	if (!chosen) {
		chosen = lib->boards.first();
	}
	m_document->undoStack()->push(new SetBoardCommand(&m_toolManager->context(), m_document->board, *chosen));
}

void MainWindow::onImportPartFileTriggered() {
	const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("部品ファイルを取り込み"), QString(),
													  QStringLiteral("部品ファイル (*.bpart *.part.json)"));
	if (path.isEmpty()) {
		return;
	}
	const auto result = m_libraryManager.importPartFile(path);
	if (!result.ok) {
		QMessageBox::warning(this, QStringLiteral("インポート失敗"), result.error);
		return;
	}
	QMessageBox::information(this, QStringLiteral("インポート完了"), QStringLiteral("マイライブラリに取り込みました。"));
}

void MainWindow::onImportBoardFileTriggered() {
	const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("基板ファイルを取り込み"), QString(),
													  QStringLiteral("基板ファイル (*.bboard)"));
	if (path.isEmpty()) {
		return;
	}
	const auto result = m_libraryManager.importBoardFile(path);
	if (!result.ok) {
		QMessageBox::warning(this, QStringLiteral("インポート失敗"), result.error);
		return;
	}
	QMessageBox::information(this, QStringLiteral("インポート完了"), QStringLiteral("マイライブラリに取り込みました。"));
}

void MainWindow::onCreateBoardTriggered() {
	BoardEditorDialog dialog(this);
	if (dialog.exec() != QDialog::Accepted) {
		return;
	}
	BoardSpec board = dialog.board();
	const QString requestedId = board.id;
	board.id = m_libraryManager.uniqueBoardIdForMyLibrary(board.id);
	const auto result = m_libraryManager.addBoardToMyLibrary(board);
	if (!result.ok) {
		QMessageBox::warning(this, QStringLiteral("作成失敗"), result.error);
		return;
	}

	QString message = QStringLiteral("マイライブラリに保存しました。");
	if (board.id != requestedId) {
		message += QStringLiteral("\n(ID が重複していたため「%1」に変更されました)").arg(board.id);
	}
	message += QStringLiteral("\n\n現在のデザインの基板として使用しますか？");
	const auto applyBtn = QMessageBox::question(this, QStringLiteral("基板を作成しました"), message);
	if (applyBtn == QMessageBox::Yes) {
		m_document->undoStack()->push(new SetBoardCommand(&m_toolManager->context(), m_document->board, board));
	}
}

void MainWindow::onCreatePartTriggered() {
	PartEditorDialog dialog(this);
	if (dialog.exec() != QDialog::Accepted) {
		return;
	}
	Part part = dialog.part();
	const QString requestedId = part.id;
	part.id = m_libraryManager.uniquePartIdForMyLibrary(part.id);
	const auto result = m_libraryManager.addPartToMyLibrary(part);
	if (!result.ok) {
		QMessageBox::warning(this, QStringLiteral("作成失敗"), result.error);
		return;
	}

	QString message = QStringLiteral("マイライブラリに保存しました。");
	if (part.id != requestedId) {
		message += QStringLiteral("\n(ID が重複していたため「%1」に変更されました)").arg(part.id);
	}
	QMessageBox::information(this, QStringLiteral("部品を作成しました"), message);
}

void MainWindow::onManageLibrariesTriggered() {
	LibraryManagerDialog dialog(&m_libraryManager, this);
	dialog.exec();
}

void MainWindow::onPartActivated(const QString &libraryId, const QString &partId) {
	// 部品パレットをダブルクリックすると配置ツールに切り替わり、以後クリックした
	// 位置に置けるようになる (Esc 相当は「選択」ツールボタンに切り替えること)。
	m_toolManager->activatePlacePartTool(libraryId, partId);
}

void MainWindow::onDrcFindingActivated(const DrcFinding &finding) {
	const QPointF target(finding.pos);
	m_frontView->centerOn(target);
	m_backView->centerOn(target);

	m_frontScene->clearSelection();
	m_backScene->clearSelection();
	if (!finding.relatedPlacementUuid.isEmpty()) {
		if (auto *item = m_frontScene->placementItemFor(finding.relatedPlacementUuid)) {
			item->setSelected(true);
		}
		if (auto *item = m_backScene->placementItemFor(finding.relatedPlacementUuid)) {
			item->setSelected(true);
		}
	}
	if (!finding.relatedWireUuid.isEmpty()) {
		if (auto *item = m_frontScene->wireItemFor(finding.relatedWireUuid)) {
			item->setSelected(true);
		}
		if (auto *item = m_backScene->wireItemFor(finding.relatedWireUuid)) {
			item->setSelected(true);
		}
	}
}

void MainWindow::closeEvent(QCloseEvent *event) {
	if (!maybeSaveChanges()) {
		event->ignore();
		return;
	}
	QSettings settings;
	settings.setValue("geometry", saveGeometry());
	settings.setValue("windowState", saveState());
	QMainWindow::closeEvent(event);
}

// ---------------------------------------------------------------- 保存・ダーティフラグ

QString MainWindow::displayTitle() const {
	return m_currentFilePath.isEmpty() ? QStringLiteral("無題") : QFileInfo(m_currentFilePath).fileName();
}

void MainWindow::updateWindowTitle() {
	const bool clean = !m_document || m_document->undoStack()->isClean();
	setWindowTitle(QStringLiteral("%1%2 - Boardes").arg(clean ? QString() : QStringLiteral("*"), displayTitle()));
}

void MainWindow::onUndoStackCleanChanged(bool) {
	updateWindowTitle();
}

bool MainWindow::maybeSaveChanges() {
	if (!m_document || m_document->undoStack()->isClean()) {
		return true;
	}
	const auto result = QMessageBox::question(
		this, QStringLiteral("未保存の変更があります"),
		QStringLiteral("「%1」への変更を保存しますか？").arg(displayTitle()),
		QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
	switch (result) {
	case QMessageBox::Save:
		return onSaveTriggered();
	case QMessageBox::Discard:
		return true;
	case QMessageBox::Cancel:
	default:
		return false;
	}
}

void MainWindow::onNewTriggered() {
	if (!maybeSaveChanges()) {
		return;
	}
	setDocument(std::make_unique<Document>());
}

void MainWindow::onOpenTriggered() {
	if (!maybeSaveChanges()) {
		return;
	}
	const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("開く"), QString(),
													   QStringLiteral("Boardes ファイル (*.boardes *.bpkg)"));
	if (path.isEmpty()) {
		return;
	}

	auto doc = std::make_unique<Document>();
	if (path.endsWith(QStringLiteral(".bpkg"), Qt::CaseInsensitive)) {
		documentio::LibraryImporter importer = [this](const QString &, const Library &lib) {
			m_libraryManager.installLibrary(lib);
		};
		const auto result = documentio::importPackage(path, *doc, importer);
		if (!result.ok) {
			QMessageBox::warning(this, QStringLiteral("開けませんでした"), result.error);
			return;
		}
		// .bpkg 自体は再配布用の配布物なので、上書き保存時に別の .boardes として
		// 扱う (元の .bpkg を勝手に書き換えない)。
		setDocument(std::move(doc));
	} else {
		if (!documentio::load(path, *doc)) {
			QMessageBox::warning(this, QStringLiteral("開けませんでした"),
								 QStringLiteral("ファイルを読み込めませんでした: %1").arg(path));
			return;
		}
		setDocument(std::move(doc), path);
		addToRecentFiles(path);
	}
}

bool MainWindow::onSaveTriggered() {
	if (m_currentFilePath.isEmpty()) {
		return onSaveAsTriggered();
	}
	if (!documentio::save(*m_document, m_currentFilePath)) {
		QMessageBox::warning(this, QStringLiteral("保存に失敗しました"),
							 QStringLiteral("ファイルに書き込めませんでした: %1").arg(m_currentFilePath));
		return false;
	}
	m_document->undoStack()->setClean();
	addToRecentFiles(m_currentFilePath);
	return true;
}

bool MainWindow::onSaveAsTriggered() {
	QString path = QFileDialog::getSaveFileName(this, QStringLiteral("名前を付けて保存"), m_currentFilePath,
												QStringLiteral("Boardes ファイル (*.boardes)"));
	if (path.isEmpty()) {
		return false;
	}
	if (!path.endsWith(QStringLiteral(".boardes"), Qt::CaseInsensitive)) {
		path += QStringLiteral(".boardes");
	}
	if (!documentio::save(*m_document, path)) {
		QMessageBox::warning(this, QStringLiteral("保存に失敗しました"),
							 QStringLiteral("ファイルに書き込めませんでした: %1").arg(path));
		return false;
	}
	m_currentFilePath = path;
	m_document->undoStack()->setClean();
	addToRecentFiles(path);
	updateWindowTitle();
	return true;
}

// ---------------------------------------------------------------- 最近使ったファイル

QStringList MainWindow::recentFiles() const {
	return QSettings().value(QStringLiteral("recentFiles")).toStringList();
}

void MainWindow::addToRecentFiles(const QString &path) {
	QStringList files = recentFiles();
	files.removeAll(path);
	files.prepend(path);
	while (files.size() > kMaxRecentFiles) {
		files.removeLast();
	}
	QSettings().setValue(QStringLiteral("recentFiles"), files);
	rebuildRecentFilesMenu();
}

void MainWindow::rebuildRecentFilesMenu() {
	ui->menuRecentFiles->clear();
	const QStringList files = recentFiles();
	if (files.isEmpty()) {
		QAction *empty = ui->menuRecentFiles->addAction(QStringLiteral("(なし)"));
		empty->setEnabled(false);
		return;
	}
	for (const QString &path : files) {
		QAction *action = ui->menuRecentFiles->addAction(QFileInfo(path).fileName());
		action->setToolTip(path);
		connect(action, &QAction::triggered, this, [this, path] { onOpenRecentFile(path); });
	}
}

void MainWindow::onOpenRecentFile(const QString &path) {
	if (!QFileInfo::exists(path)) {
		QMessageBox::warning(this, QStringLiteral("ファイルが見つかりません"),
							 QStringLiteral("ファイルが見つかりません: %1").arg(path));
		QStringList files = recentFiles();
		files.removeAll(path);
		QSettings().setValue(QStringLiteral("recentFiles"), files);
		rebuildRecentFilesMenu();
		return;
	}
	if (!maybeSaveChanges()) {
		return;
	}
	auto doc = std::make_unique<Document>();
	if (!documentio::load(path, *doc)) {
		QMessageBox::warning(this, QStringLiteral("開けませんでした"),
							 QStringLiteral("ファイルを読み込めませんでした: %1").arg(path));
		return;
	}
	setDocument(std::move(doc), path);
	addToRecentFiles(path);
}

// ---------------------------------------------------------------- エクスポート

void MainWindow::onExportPackageTriggered() {
	QString path = QFileDialog::getSaveFileName(this, QStringLiteral("設計パッケージとしてエクスポート"), QString(),
												QStringLiteral("Boardes パッケージ (*.bpkg)"));
	if (path.isEmpty()) {
		return;
	}
	if (!path.endsWith(QStringLiteral(".bpkg"), Qt::CaseInsensitive)) {
		path += QStringLiteral(".bpkg");
	}

	documentio::LibraryResolver resolver = [this](const QString &libId) { return m_libraryManager.library(libId); };
	const auto result = documentio::exportPackage(*m_document, resolver, path);
	if (!result.ok) {
		QMessageBox::warning(this, QStringLiteral("エクスポート失敗"), result.error);
		return;
	}

	QString message = QStringLiteral("エクスポートしました: %1").arg(path);
	if (!result.skippedLibraryIds.isEmpty()) {
		QStringList lines;
		for (const auto &libId : result.skippedLibraryIds) {
			if (libId == LibraryManager::myLibraryId()) {
				lines << QStringLiteral(
					"・マイライブラリの部品が含まれています。同梱するには「複製」でライブラリ化し、"
					"ライセンスを設定してください。");
			} else {
				const auto lib = m_libraryManager.library(libId);
				lines << QStringLiteral("・%1 は再配布不可のため同梱されませんでした。").arg(lib ? lib->name : libId);
			}
		}
		message += QStringLiteral("\n\n") + lines.join(QStringLiteral("\n"));
	}
	QMessageBox::information(this, QStringLiteral("エクスポート完了"), message);
}

std::optional<imageexport::Options> MainWindow::promptExportOptions() {
	ExportImageOptionsDialog dialog(this);
	if (dialog.exec() != QDialog::Accepted) {
		return std::nullopt;
	}
	return dialog.options();
}

void MainWindow::onExportPngTriggered() {
	const auto options = promptExportOptions();
	if (!options) {
		return;
	}
	QString path = QFileDialog::getSaveFileName(this, QStringLiteral("画像として保存 (PNG)"), QString(),
												QStringLiteral("PNG 画像 (*.png)"));
	if (path.isEmpty()) {
		return;
	}
	if (!path.endsWith(QStringLiteral(".png"), Qt::CaseInsensitive)) {
		path += QStringLiteral(".png");
	}
	if (!imageexport::saveAsPng(m_frontScene, m_backScene, *options, path)) {
		QMessageBox::warning(this, QStringLiteral("エクスポート失敗"), QStringLiteral("PNG の書き出しに失敗しました。"));
	}
}

void MainWindow::onExportSvgTriggered() {
	const auto options = promptExportOptions();
	if (!options) {
		return;
	}
	QString path = QFileDialog::getSaveFileName(this, QStringLiteral("画像として保存 (SVG)"), QString(),
												QStringLiteral("SVG 画像 (*.svg)"));
	if (path.isEmpty()) {
		return;
	}
	if (!path.endsWith(QStringLiteral(".svg"), Qt::CaseInsensitive)) {
		path += QStringLiteral(".svg");
	}
	if (!imageexport::saveAsSvg(m_frontScene, m_backScene, *options, path)) {
		QMessageBox::warning(this, QStringLiteral("エクスポート失敗"), QStringLiteral("SVG の書き出しに失敗しました。"));
	}
}

void MainWindow::onExportClipboardTriggered() {
	const auto options = promptExportOptions();
	if (!options) {
		return;
	}
	const QImage img = imageexport::render(m_frontScene, m_backScene, *options);
	if (img.isNull()) {
		QMessageBox::warning(this, QStringLiteral("コピー失敗"), QStringLiteral("画像の生成に失敗しました。"));
		return;
	}
	QApplication::clipboard()->setImage(img);
}
