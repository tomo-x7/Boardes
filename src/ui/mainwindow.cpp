#include "mainwindow.h"

#include <QAbstractButton>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QSet>
#include <QSettings>
#include <QSplitter>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <algorithm>
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
#include "exportpackagedialog.h"
#include "helphint.h"
#include "input/keymapdialog.h"
#include "librarymanagerdialog.h"
#include "toolbarcustomizedialog.h"
#include "toolbarlayout.h"
#include "objectlistpanel.h"
#include "parteditordialog.h"
#include "partselector.h"
#include "statspanel.h"
#include "ui_mainwindow.h"
#include "viewlink.h"
#include "zoombar.h"

namespace {
constexpr int kMaxRecentFiles = 10;
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow) {
	ui->setupUi(this);

	connect(ui->aboutAction, &QAction::triggered, this, &MainWindow::onAboutTriggered);
	connect(ui->manageLibrariesAction, &QAction::triggered, this, &MainWindow::onManageLibrariesTriggered);
	setupFileMenuConnections();

	// Esc はウィンドウ内のどこにフォーカスがあっても効くようにする (WindowShortcut)。
	// ツールに途中状態があればそれだけ破棄し、無ければ選択ツールへ戻る (ToolManager::cancelCurrent)。
	auto *escapeAction = new QAction(this);
	escapeAction->setShortcut(QKeySequence(Qt::Key_Escape));
	escapeAction->setShortcutContext(Qt::WindowShortcut);
	addAction(escapeAction);
	connect(escapeAction, &QAction::triggered, this, &MainWindow::onEscapeTriggered);

	const QSettings settings;
	if (settings.contains("geometry")) {
		restoreGeometry(settings.value("geometry").toByteArray());
	}
	if (settings.contains("windowState")) {
		restoreState(settings.value("windowState").toByteArray());
	}

	m_libraryManager.loadAll();
	m_keymap.load();

	setupCentralWidget();
	m_frontView->setKeymap(&m_keymap);
	m_backView->setKeymap(&m_keymap);
	setupSidePanelDock();
	setDocument(std::make_unique<Document>());

	ToolContext ctx;
	ctx.document = m_document.get();
	ctx.libraryManager = &m_libraryManager;
	ctx.frontScene = m_frontScene;
	ctx.backScene = m_backScene;
	ctx.snapEngine = &m_snapEngine;
	ctx.keymap = &m_keymap;
	m_toolManager = std::make_unique<ToolManager>(ctx, this);
	m_frontScene->setToolManager(m_toolManager.get());
	m_backScene->setToolManager(m_toolManager.get());
	// ObjectListPanel は Tool と同じく ToolContext を直接持ち、選択操作・Undo コマンドの
	// push を自分で行う。m_toolManager が持つ ToolContext は以後ずっと同じ実体なので、
	// ここで一度渡せば以降 (ドキュメント切替を含め) ずっと有効。
	if (m_objectListPanel) {
		m_objectListPanel->setContext(&m_toolManager->context());
	}
	connect(m_toolManager.get(), &ToolManager::statusHintChanged, this,
			[this](const QString &hint) { ui->statusbar->showMessage(hint); });
	auto *selectionSummaryLabel = new QLabel(this);
	ui->statusbar->addPermanentWidget(selectionSummaryLabel);
	connect(m_toolManager.get(), &ToolManager::selectionSummaryChanged, selectionSummaryLabel, &QLabel::setText);

	setupViewMenuAndToolbar();
	setupToolsToolbar();
	// 保存されているツールバー構成 (表示/非表示・ボタンスタイル・ユーザー作成分) を反映する。
	// builtin (表示/ツール) の項目自体は上の2関数が作った並びのまま (Phase 19)。
	rebuildToolbars();
	rebuildRecentFilesMenu();
	updateWindowTitle();

	m_partSelector->setLibraryManager(&m_libraryManager);
	connect(m_partSelector, &PartSelector::partSelected, this, &MainWindow::onPartSelected);
	connect(m_toolManager.get(), &ToolManager::pendingPartCleared, m_partSelector, &PartSelector::clearPendingPart);

	QTimer::singleShot(0, this, &MainWindow::checkLibraryLoadIssues);
}

void MainWindow::checkLibraryLoadIssues() {
	const auto issues = m_libraryManager.loadIssues();
	if (issues.isEmpty()) {
		return;
	}
	QStringList lines;
	for (const auto &issue : issues) {
		lines << QStringLiteral("・%1 : %2").arg(QFileInfo(issue.directory).fileName(), issue.reason);
	}
	const auto btn = QMessageBox::question(
		this, QStringLiteral("破損したライブラリがあります"),
		QStringLiteral("次のライブラリを読み込めませんでした:\n%1\n\n"
					   "削除すると、これらを参照している設計データは開いたときに部品が解決できなくなります。\n"
					   "削除しますか？")
			.arg(lines.join(QStringLiteral("\n"))),
		QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
	if (btn != QMessageBox::Yes) {
		return;
	}
	for (const auto &issue : issues) {
		QDir(issue.directory).removeRecursively();
	}
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

	m_viewLink = new ViewLinkController(this);
	m_viewLink->setViews(m_frontView, m_backView);
}

void MainWindow::setupSidePanelDock() {
	auto *dock = new QDockWidget(QStringLiteral("パネル"), this);
	dock->setObjectName(QStringLiteral("sidePanelDock"));

	auto *tabs = new QTabWidget(dock);
	m_objectListPanel = new ObjectListPanel(tabs);
	m_drcPanel = new DrcPanel(tabs);
	m_statsPanel = new StatsPanel(tabs);
	tabs->addTab(m_objectListPanel, QStringLiteral("オブジェクト"));
	tabs->addTab(m_drcPanel, QStringLiteral("DRC"));
	tabs->addTab(m_statsPanel, QStringLiteral("統計"));
	dock->setWidget(tabs);
	addDockWidget(Qt::RightDockWidgetArea, dock);
	ui->menuView->addAction(dock->toggleViewAction());

	connect(m_drcPanel, &DrcPanel::findingActivated, this, &MainWindow::onDrcFindingActivated);
	// ライブラリ側の変更 (部品の追加・削除等) は Undo スタックを経由しないので、
	// undoStack::indexChanged だけでは検知できない。ここで明示的に繋いでおく
	// (DRC の「再検査」ボタンを廃止した分の埋め合わせ、Phase 15)。
	connect(&m_libraryManager, &LibraryManager::librariesChanged, m_drcPanel, &DrcPanel::refresh);
	connect(&m_libraryManager, &LibraryManager::librariesChanged, m_statsPanel, &StatsPanel::refresh);
	connect(&m_libraryManager, &LibraryManager::librariesChanged, m_objectListPanel, &ObjectListPanel::refresh);
}

void MainWindow::setupViewMenuAndToolbar() {
	auto *toolbar = addToolBar(QStringLiteral("表示"));
	toolbar->setObjectName(QStringLiteral("viewToolBar"));
	m_toolbars.insert(QStringLiteral("view"), toolbar);

	QAction *zoomInAction = toolbar->addAction(QStringLiteral("拡大"));
	connect(zoomInAction, &QAction::triggered, this, [this] {
		m_frontView->zoomIn();
		m_backView->zoomIn();
	});
	m_actionRegistry.add(QStringLiteral("toolbar.view.zoomIn"), zoomInAction);
	QAction *zoomOutAction = toolbar->addAction(QStringLiteral("縮小"));
	connect(zoomOutAction, &QAction::triggered, this, [this] {
		m_frontView->zoomOut();
		m_backView->zoomOut();
	});
	m_actionRegistry.add(QStringLiteral("toolbar.view.zoomOut"), zoomOutAction);
	QAction *zoomResetAction = toolbar->addAction(QStringLiteral("100%"));
	connect(zoomResetAction, &QAction::triggered, this, [this] {
		m_frontView->resetZoom();
		m_backView->resetZoom();
	});
	m_actionRegistry.add(QStringLiteral("toolbar.view.zoomReset"), zoomResetAction);
	QAction *fitAction = toolbar->addAction(QStringLiteral("全体表示"));
	fitAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
	connect(fitAction, &QAction::triggered, this, [this] {
		m_frontView->fitBoardToWindow();
		m_backView->fitBoardToWindow();
	});
	m_actionRegistry.add(QStringLiteral("toolbar.view.fit"), fitAction);

	toolbar->addSeparator();

	QAction *orientationAction = toolbar->addAction(QStringLiteral("表裏レイアウト切替"));
	connect(orientationAction, &QAction::triggered, this, &MainWindow::onToggleViewOrientation);
	m_actionRegistry.add(QStringLiteral("toolbar.view.orientation"), orientationAction);

	QAction *linkViewsAction = toolbar->addAction(QStringLiteral("表裏のビューを連動"));
	m_actionRegistry.add(QStringLiteral("toolbar.view.linkViews"), linkViewsAction);
	linkViewsAction->setCheckable(true);
	linkViewsAction->setToolTip(
		QStringLiteral("ON にすると、表裏どちらかのビューをスクロール/ズームしたとき、"
					   "もう片方も同じ位置・倍率に追従します。"));
	{
		const bool linkViews = QSettings().value(QStringLiteral("view/linkViews"), false).toBool();
		linkViewsAction->setChecked(linkViews);
		m_viewLink->setEnabled(linkViews);
	}
	connect(linkViewsAction, &QAction::toggled, this, [this](bool v) {
		m_viewLink->setEnabled(v);
		QSettings().setValue(QStringLiteral("view/linkViews"), v);
	});
	ui->menuView->addAction(linkViewsAction);

	toolbar->addSeparator();

	// 裏面ビューの見え方 (排他3択)。
	auto *backViewModeGroup = new QActionGroup(this);
	backViewModeGroup->setExclusive(true);
	auto *backViewMenu = new QMenu(QStringLiteral("裏面の見え方"), this);
	auto addBackViewModeAction = [&](const QString &text, BackViewMode mode) {
		QAction *a = backViewMenu->addAction(text);
		a->setCheckable(true);
		a->setData(static_cast<int>(mode));
		backViewModeGroup->addAction(a);
		connect(a, &QAction::triggered, this, [this, mode] {
			m_frontScene->setBackViewMode(mode);
			m_backScene->setBackViewMode(mode);
			QSettings().setValue(QStringLiteral("view/backViewMode"), static_cast<int>(mode));
		});
	};
	addBackViewModeAction(QStringLiteral("裏から見た向き (左右反転)"), BackViewMode::MirrorX);
	addBackViewModeAction(QStringLiteral("透過した向き (左右そのまま)"), BackViewMode::Through);
	addBackViewModeAction(QStringLiteral("裏から見た向き (上下反転)"), BackViewMode::MirrorY);
	ui->menuView->addMenu(backViewMenu);
	{
		const auto savedMode =
			static_cast<BackViewMode>(QSettings().value(QStringLiteral("view/backViewMode"), 0).toInt());
		m_frontScene->setBackViewMode(savedMode);
		m_backScene->setBackViewMode(savedMode);
		for (QAction *a : backViewModeGroup->actions()) {
			if (a->data().toInt() == static_cast<int>(savedMode)) {
				a->setChecked(true);
			}
		}
	}

	toolbar->addSeparator();

	auto addToggle = [&](const QString &text, bool checked, const std::function<void(bool)> &onToggled,
						 const QString &tooltip = QString()) -> QAction * {
		QAction *a = toolbar->addAction(text);
		a->setCheckable(true);
		a->setChecked(checked);
		if (!tooltip.isEmpty()) {
			a->setToolTip(tooltip);
		}
		connect(a, &QAction::toggled, this, onToggled);
		ui->menuView->addAction(a);
		return a;
	};

	m_actionRegistry.add(
		QStringLiteral("toolbar.view.wireFrontBare"),
		addToggle(
			QStringLiteral("表面配線"), true,
			[this](bool v) {
				m_frontScene->setLayerVisible(WireLayer::FrontBare, v);
				m_backScene->setLayerVisible(WireLayer::FrontBare, v);
			},
			QStringLiteral("表面の裸線 (被覆なし配線) の表示/非表示を切り替えます。")));
	m_actionRegistry.add(
		QStringLiteral("toolbar.view.wireBackBare"),
		addToggle(
			QStringLiteral("裏面配線"), true,
			[this](bool v) {
				m_frontScene->setLayerVisible(WireLayer::BackBare, v);
				m_backScene->setLayerVisible(WireLayer::BackBare, v);
			},
			QStringLiteral("裏面の裸線 (被覆なし配線) の表示/非表示を切り替えます。")));
	m_actionRegistry.add(
		QStringLiteral("toolbar.view.wireFrontInsulated"),
		addToggle(
			QStringLiteral("表面被覆配線"), true,
			[this](bool v) {
				m_frontScene->setLayerVisible(WireLayer::FrontInsulated, v);
				m_backScene->setLayerVisible(WireLayer::FrontInsulated, v);
			},
			QStringLiteral("表面の被覆配線の表示/非表示を切り替えます。")));
	m_actionRegistry.add(
		QStringLiteral("toolbar.view.wireBackInsulated"),
		addToggle(
			QStringLiteral("裏面被覆配線"), true,
			[this](bool v) {
				m_frontScene->setLayerVisible(WireLayer::BackInsulated, v);
				m_backScene->setLayerVisible(WireLayer::BackInsulated, v);
			},
			QStringLiteral("裏面の被覆配線の表示/非表示を切り替えます。")));
	m_actionRegistry.add(
		QStringLiteral("toolbar.view.outline"),
		addToggle(
			QStringLiteral("外形線"), true,
			[this](bool v) {
				m_frontScene->setLayerVisible(WireLayer::Outline, v);
				m_backScene->setLayerVisible(WireLayer::Outline, v);
			},
			QStringLiteral("基板外形線の表示/非表示を切り替えます。")));
	m_actionRegistry.add(
		QStringLiteral("toolbar.view.partOutline"),
		addToggle(
			QStringLiteral("部品アウトライン表示"), false,
			[this](bool v) {
				m_frontScene->setForcePartOutline(v);
				m_backScene->setForcePartOutline(v);
			},
			QStringLiteral("ON にすると、自面の部品もアートワークではなく輪郭矩形+ピンパッドだけで表示します。")));
	m_actionRegistry.add(QStringLiteral("toolbar.view.pinNumbers"),
						 addToggle(
							 QStringLiteral("ピン番号表示"), false,
							 [this](bool v) {
								 m_frontScene->setShowPinNumbers(v);
								 m_backScene->setShowPinNumbers(v);
							 },
							 QStringLiteral("部品のピン番号を表示します。")));
	m_actionRegistry.add(QStringLiteral("toolbar.view.labels"),
						 addToggle(
							 QStringLiteral("部品番号/値表示"), true,
							 [this](bool v) {
								 m_frontScene->setShowLabels(v);
								 m_backScene->setShowLabels(v);
							 },
							 QStringLiteral("部品の refDes (No) と値のラベル表示を切り替えます。")));

	// 接点 (ピン) マーカー。PasS 部品は赤いマーカー画素をそのまま残しているので、ON にすると
	// その上にさらに丸が重なる。独自部品でも同じ位置にピンがあれば表示できる。
	const bool pinMarkersVisible = QSettings().value(QStringLiteral("view/pinMarkerVisible"), true).toBool();
	QColor pinMarkerColor = QSettings().value(QStringLiteral("view/pinMarkerColor"), QColor(255, 0, 0)).value<QColor>();
	if (!pinMarkerColor.isValid()) {
		pinMarkerColor = QColor(255, 0, 0);
	}
	m_frontScene->setPinMarkers(pinMarkersVisible, pinMarkerColor, 3.0);
	m_backScene->setPinMarkers(pinMarkersVisible, pinMarkerColor, 3.0);
	m_actionRegistry.add(
		QStringLiteral("toolbar.view.pinMarkers"),
		addToggle(
			QStringLiteral("接点マーカーを表示"), pinMarkersVisible,
			[this](bool v) {
				const QColor c =
					QSettings().value(QStringLiteral("view/pinMarkerColor"), QColor(255, 0, 0)).value<QColor>();
				m_frontScene->setPinMarkers(v, c.isValid() ? c : QColor(255, 0, 0), 3.0);
				m_backScene->setPinMarkers(v, c.isValid() ? c : QColor(255, 0, 0), 3.0);
				QSettings().setValue(QStringLiteral("view/pinMarkerVisible"), v);
			},
			QStringLiteral("PasS 部品の赤マーカー等、ピンの位置に丸を重ねて表示します。独自部品でも表示できます。")));
	QAction *pinMarkerColorAction = toolbar->addAction(QStringLiteral("接点マーカーの色..."));
	pinMarkerColorAction->setToolTip(QStringLiteral("接点マーカーの色を選びます。"));
	connect(pinMarkerColorAction, &QAction::triggered, this, [this] {
		const QColor cur =
			QSettings().value(QStringLiteral("view/pinMarkerColor"), QColor(255, 0, 0)).value<QColor>();
		const QColor c = QColorDialog::getColor(cur.isValid() ? cur : QColor(255, 0, 0), this, tr("接点マーカーの色を選択"));
		if (!c.isValid()) {
			return;
		}
		QSettings().setValue(QStringLiteral("view/pinMarkerColor"), c);
		const bool visible = QSettings().value(QStringLiteral("view/pinMarkerVisible"), true).toBool();
		m_frontScene->setPinMarkers(visible, c, 3.0);
		m_backScene->setPinMarkers(visible, c, 3.0);
	});
	ui->menuView->addAction(pinMarkerColorAction);
	m_actionRegistry.add(QStringLiteral("toolbar.view.pinMarkerColor"), pinMarkerColorAction);

	m_zoomBar = new ZoomBar(this);
	ui->statusbar->addPermanentWidget(m_zoomBar);
	connect(m_frontView, &BoardView::focusReceived, this, [this](BoardView *v) { m_zoomBar->setTargetView(v); });
	connect(m_backView, &BoardView::focusReceived, this, [this](BoardView *v) { m_zoomBar->setTargetView(v); });
	m_zoomBar->setTargetView(m_frontView);

	// 表示メニューの末尾: ツールバー・ドックの表示/非表示 (LibreOffice 同様、メニューバー
	// 自体はカスタマイズ対象外) と、カスタマイズ系ダイアログ。
	ui->menuView->addSeparator();
	ui->menuView->addAction(toolbar->toggleViewAction());
	QAction *customizeShortcutsAction = ui->menuView->addAction(QStringLiteral("操作のカスタマイズ..."));
	customizeShortcutsAction->setToolTip(
		QStringLiteral("ショートカットキーやマウス操作の割り当てを変更します。左手デバイス等の独自キーにも対応します。"));
	connect(customizeShortcutsAction, &QAction::triggered, this, [this] {
		KeymapDialog dlg(&m_keymap, this);
		dlg.exec();
	});
	QAction *customizeToolbarsAction = ui->menuView->addAction(QStringLiteral("ツールバーのカスタマイズ..."));
	customizeToolbarsAction->setToolTip(
		QStringLiteral("ツールバーの項目・並び順・表示スタイルを変更したり、独自のツールバーを作成したりします。"));
	connect(customizeToolbarsAction, &QAction::triggered, this, [this] {
		ToolbarCustomizeDialog dlg(&m_actionRegistry, this);
		connect(&dlg, &ToolbarCustomizeDialog::layoutsChanged, this, &MainWindow::rebuildToolbars);
		dlg.exec();
	});
}

void MainWindow::setupToolsToolbar() {
	auto *toolbar = addToolBar(QStringLiteral("ツール"));
	toolbar->setObjectName(QStringLiteral("toolsToolBar"));
	insertToolBarBreak(toolbar);  // 表示ツールバーとは別行に
	m_toolbars.insert(QStringLiteral("tools"), toolbar);

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
	m_actionRegistry.add(QStringLiteral("toolbar.tools.select"), selectAction);
	toolbar->addSeparator();
	// どちらの面に描くかはボタンでは選ばない (表面/裏面どちらのエディタで描いたかで
	// 自動的に決まる)。ユーザーが選ぶのは線の種類だけ。
	m_actionRegistry.add(QStringLiteral("toolbar.tools.wireBare"),
						 addToolAction(QStringLiteral("配線"), [this] { m_toolManager->activateWireTool(WireKind::Bare); }));
	m_actionRegistry.add(
		QStringLiteral("toolbar.tools.wireInsulated"),
		addToolAction(QStringLiteral("被覆配線"), [this] { m_toolManager->activateWireTool(WireKind::Insulated); }));
	m_actionRegistry.add(
		QStringLiteral("toolbar.tools.wireOutline"),
		addToolAction(QStringLiteral("外形線"), [this] { m_toolManager->activateWireTool(WireKind::Outline); }));
	toolbar->addSeparator();
	m_actionRegistry.add(QStringLiteral("toolbar.tools.draft"),
						 addToolAction(QStringLiteral("下書き"), [this] { m_toolManager->activateDraftTool(); }));

	toolbar->addSeparator();
	auto *snapCombo = new QComboBox();
	snapCombo->addItem(QStringLiteral("フル (2.54mm)"), static_cast<int>(units::Granularity::Full));
	snapCombo->addItem(QStringLiteral("ハーフ (1.27mm)"), static_cast<int>(units::Granularity::Half));
	snapCombo->addItem(QStringLiteral("フリー"), static_cast<int>(units::Granularity::Free));
	connect(snapCombo, &QComboBox::currentIndexChanged, this, [this, snapCombo](int) {
		m_snapEngine.setGranularity(static_cast<units::Granularity>(snapCombo->currentData().toInt()));
	});
	helphint::attach(snapCombo, QStringLiteral("配線を引くときのスナップ粒度です。フル=2.54mmグリッド、"
											  "ハーフ=1.27mm、フリー=任意の位置に置けます。"));
	toolbar->addWidget(snapCombo);

	ui->menuView->addAction(toolbar->toggleViewAction());
}

void MainWindow::rebuildToolbars() {
	const auto layouts = toolbarlayout::load();
	QSet<QString> wantedIds;
	for (const auto &layout : layouts) {
		wantedIds.insert(layout.id);
		QToolBar *toolbar = m_toolbars.value(layout.id);
		if (!toolbar) {
			if (layout.builtin) {
				// 表示/ツールツールバーは setupViewMenuAndToolbar()/setupToolsToolbar() が
				// 必ず先に作っているはず。無ければ何もしない (安全側に倒す)。
				continue;
			}
			// ユーザーが「新しいツールバー」で作成した分。
			toolbar = addToolBar(layout.title);
			toolbar->setObjectName(layout.id);
			m_toolbars.insert(layout.id, toolbar);
		}
		toolbar->setWindowTitle(layout.title);
		toolbar->setToolButtonStyle(layout.style);
		toolbar->setVisible(layout.visible);
		if (!layout.builtin) {
			// builtin ではないツールバーは特別なウィジェットを含まないので、
			// 毎回安全に組み立て直せる。
			toolbar->clear();
			for (const auto &item : layout.items) {
				if (item == QStringLiteral("-")) {
					toolbar->addSeparator();
				} else if (QAction *a = m_actionRegistry.action(item)) {
					toolbar->addAction(a);
				}
				// 未知の commandId は無視する (前方互換。Phase 17 の方針)。
			}
		}
		// builtin (表示/ツール) は項目の並びを変えない — スナップ粒度コンボ等の
		// 特殊ウィジェットを安全に組み立て直す手段が無いため、表示/非表示と
		// ボタンスタイルのみカスタマイズ対象にしている (ToolbarCustomizeDialog 参照)。
	}
	// レイアウトから消えた (ユーザーが削除した) カスタムツールバーを片付ける。
	for (auto it = m_toolbars.begin(); it != m_toolbars.end();) {
		if (!wantedIds.contains(it.key())) {
			removeToolBar(it.value());
			it.value()->deleteLater();
			it = m_toolbars.erase(it);
		} else {
			++it;
		}
	}
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
	// Document::board の実体アドレスを渡す。SetBoardCommand は同じ Document インスタンスの
	// board を書き換えるだけ (アドレスは変わらない) なので、ドキュメント切替時にだけ呼べば足りる。
	m_snapEngine.setBoard(&m_document->board);
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
	if (m_objectListPanel) {
		connect(m_document->undoStack(), &QUndoStack::indexChanged, m_objectListPanel,
				[this](int) { m_objectListPanel->refresh(); });
		m_objectListPanel->refresh();
	}

	syncAllScenes();
	updateWindowTitle();
	fitViewsDeferred();
}

void MainWindow::syncAllScenes() {
	m_frontScene->setDocument(m_document.get(), &m_libraryManager);
	m_backScene->setDocument(m_document.get(), &m_libraryManager);
}

void MainWindow::pushBoardAndFit(const BoardSpec &board) {
	m_document->undoStack()->push(new SetBoardCommand(&m_toolManager->context(), m_document->board, board));
	fitViewsDeferred();
}

void MainWindow::fitViewsDeferred() {
	// 0ms 遅延はウィジェットのサイズ確定 (レイアウト計算) を待つため。基板が空
	// (size が未設定) のときは全体表示ではなく見やすい既定倍率にする
	// (以前は初期倍率が低すぎて何も見えない状態だったため)。
	QTimer::singleShot(0, this, [this] {
		if (!m_document) {
			return;
		}
		if (m_document->board.size.isEmpty()) {
			m_frontView->setZoom(4.0);
			m_backView->setZoom(4.0);
			return;
		}
		m_frontView->fitBoardToWindow();
		m_backView->fitBoardToWindow();
	});
}

void MainWindow::onAboutTriggered() {
	About about(this);
	about.exec();
}

void MainWindow::onToggleViewOrientation() {
	m_viewsSplitter->setOrientation(m_viewsSplitter->orientation() == Qt::Horizontal ? Qt::Vertical : Qt::Horizontal);
}

void MainWindow::onManageLibrariesTriggered() {
	// Phase 14: 取込・作成・編集・削除はすべてこのダイアログに集約されている
	// (ファイル→インポート、ライブラリ→新規作成メニューは廃止した)。
	LibraryManagerDialog dialog(&m_libraryManager, this);
	connect(&dialog, &LibraryManagerDialog::boardApplyRequested, this,
			[this](const BoardSpec &board) { pushBoardAndFit(board); });
	dialog.exec();
}

void MainWindow::onPartSelected(const QString &libraryId, const QString &partId) {
	// 部品パレットをシングルクリックすると配置ツールに切り替わり、以後クリックした
	// 位置に置けるようになる (右クリックまたは Esc で選択ツールに戻る)。キー入力が
	// 引き続きビューへ届くよう、フォーカスをビューに戻しておく。
	m_toolManager->activatePlacePartTool(libraryId, partId);
	m_frontView->setFocus();
}

void MainWindow::onEscapeTriggered() {
	if (m_toolManager) {
		m_toolManager->cancelCurrent();
	}
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
			showCorruptedFileDialog(path, LoadResult::failure(QStringLiteral("ファイルを読み込めません"), result.error));
			return;
		}
		// .bpkg 自体は再配布用の配布物なので、上書き保存時に別の .boardes として
		// 扱う (元の .bpkg を勝手に書き換えない)。
		setDocument(std::move(doc));
	} else {
		const LoadResult result = documentio::load(path, *doc);
		if (!result) {
			showCorruptedFileDialog(path, result);
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
	const LoadResult result = documentio::load(path, *doc);
	if (!result) {
		showCorruptedFileDialog(path, result);
		return;
	}
	setDocument(std::move(doc), path);
	addToRecentFiles(path);
}

void MainWindow::showCorruptedFileDialog(const QString &path, const LoadResult &result) {
	QMessageBox box(this);
	box.setIcon(QMessageBox::Warning);
	box.setWindowTitle(result.summary.isEmpty() ? QStringLiteral("ファイルを読み込めません") : result.summary);
	box.setText(QStringLiteral("「%1」は Boardes のデータとして読み込めませんでした。").arg(QFileInfo(path).fileName()));
	if (!result.detail.isEmpty()) {
		box.setInformativeText(QStringLiteral("理由: %1").arg(result.detail));
	}
	QPushButton *deleteButton = box.addButton(QStringLiteral("ファイルを削除"), QMessageBox::DestructiveRole);
	QPushButton *copyButton = box.addButton(QStringLiteral("詳細をコピー"), QMessageBox::ActionRole);
	QPushButton *closeButton = box.addButton(QStringLiteral("閉じる"), QMessageBox::RejectRole);
	box.setDefaultButton(closeButton);
	box.exec();

	if (box.clickedButton() == static_cast<QAbstractButton *>(copyButton)) {
		QApplication::clipboard()->setText(
			QStringLiteral("%1\n%2\n%3").arg(path, result.summary, result.detail));
	} else if (box.clickedButton() == static_cast<QAbstractButton *>(deleteButton)) {
		const auto btn =
			QMessageBox::question(this, QStringLiteral("確認"), QStringLiteral("「%1」を削除します。よろしいですか？").arg(path));
		if (btn == QMessageBox::Yes) {
			if (!QFile::moveToTrash(path)) {
				QFile::remove(path);
			}
			QStringList files = recentFiles();
			files.removeAll(path);
			QSettings().setValue(QStringLiteral("recentFiles"), files);
			rebuildRecentFilesMenu();
		}
	}
}

// ---------------------------------------------------------------- エクスポート

void MainWindow::onExportPackageTriggered() {
	const QSet<QString> requiredIds = documentio::requiredLibraryIds(*m_document);
	QStringList requiredIdList(requiredIds.begin(), requiredIds.end());
	std::sort(requiredIdList.begin(), requiredIdList.end());

	QSet<QString> includeIds;
	if (!requiredIdList.isEmpty()) {
		// 同梱するライブラリをユーザーに選ばせる (Phase 14: 再配布不可でも「見送る」
		// 選択を明示させる。自動でスキップしていた旧挙動をやめた)。
		ExportPackageDialog exportDialog(&m_libraryManager, requiredIdList, this);
		if (exportDialog.exec() != QDialog::Accepted) {
			return;
		}
		includeIds = exportDialog.includeLibraryIds();
	}

	QString path = QFileDialog::getSaveFileName(this, QStringLiteral("設計パッケージとしてエクスポート"), QString(),
												QStringLiteral("Boardes パッケージ (*.bpkg)"));
	if (path.isEmpty()) {
		return;
	}
	if (!path.endsWith(QStringLiteral(".bpkg"), Qt::CaseInsensitive)) {
		path += QStringLiteral(".bpkg");
	}

	documentio::LibraryResolver resolver = [this](const QString &libId) { return m_libraryManager.library(libId); };
	const auto result = documentio::exportPackage(*m_document, resolver, includeIds, path);
	if (!result.ok) {
		QMessageBox::warning(this, QStringLiteral("エクスポート失敗"), result.error);
		return;
	}

	QString message = QStringLiteral("エクスポートしました: %1").arg(path);
	if (!result.skippedLibraryIds.isEmpty()) {
		QStringList lines;
		for (const auto &libId : result.skippedLibraryIds) {
			const auto lib = m_libraryManager.library(libId);
			lines << QStringLiteral("・%1 は同梱しませんでした。").arg(lib ? lib->name : libId);
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
