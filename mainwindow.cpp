#include "mainwindow.h"

#include <QCloseEvent>
#include <QGraphicsItem>
#include <QLabel>
#include <QMessageBox>
#include <QSettings>

#include "./about.h"
#include "./placedcomponent.h"
#include "./ui_mainwindow.h"

enum ItemSelectorType {
	ALL,
};

void constructItemSelectorView(Ui::MainWindow *ui,
							   LibManager &libManager,
							   QWidget *parent,
							   ItemSelectorType type)
{
	switch (type) {
	case ItemSelectorType::ALL: {
		for (auto &[key, lib] : libManager.libraries) {
			for (auto &[key, category] : lib.categories) {
				for (auto &[key, compptr] : category.components) {
					ui->itemSelectLayout->addWidget(
						new QLabel(QString::fromStdString(compptr->name), parent));
				}
			}
		}
		break;
	}
	default:
		break;
	}
}

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
	, ui(new Ui::MainWindow)
	, frontScene(new QGraphicsScene(this))
	, backScene(new QGraphicsScene(this))
	, libManager()
{
	ui->setupUi(this);

	QSettings settings;
	if (settings.contains("geometry")) {
		restoreGeometry(settings.value("geometry").toByteArray());
	} else {
	}
	if (settings.contains("windowState")) {
		restoreState(settings.value("windowState").toByteArray());
	}

	constructItemSelectorView(this->ui, this->libManager, parent, ItemSelectorType::ALL);

	auto compptr = libManager.libraries["testlib"].categories["testcat"].components["testitem"];
	auto placed = new PlacedComponent(frontScene, backScene, compptr);

	// frontScene->setSceneRect(0, 0, 400, 300);
	// backScene->setSceneRect(0, 0, 400, 300);
	// frontScene->addRect(0, 0, 400, 300);

	// auto c1 = new PlacedComponent(frontScene, backScene);
	// dataList.push_front(c1);

	ui->frontView->setScene(frontScene);
	ui->backView->setScene(backScene);
}

MainWindow::~MainWindow()
{
	delete ui;
}

void MainWindow::on_aboutAction_triggered()
{
	auto about = About();
	about.exec();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
	if (false) {
		auto message = QMessageBox(QMessageBox::Question,
								   "未保存の変更があります",
								   "保存しますか？",
								   QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
								   this);
		auto result = message.exec();
		switch (result) {
		case QMessageBox::Save:
			break;
		case QMessageBox::Discard:
			break;
		case QMessageBox::Cancel:
		default:
			event->ignore();
			return;
		}
	}

	QSettings settings;
	settings.setValue("geometry", saveGeometry());
	settings.setValue("windowState", saveState());
	QMainWindow::closeEvent(event);
}

void MainWindow::on_itemSelectType_currentIndexChanged(int index)
{
	// パーツ選択ビューのタイプが更新された、表示を変更する
}
