#include "mainwindow.h"

#include <QCloseEvent>
#include <QGraphicsItem>
#include <QMessageBox>
#include <QSettings>

#include "./about.h"
#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
	, ui(new Ui::MainWindow)
	, frontScene(new QGraphicsScene(this))
	, backScene(new QGraphicsScene(this))
{
    ui->setupUi(this);

	QSettings settings;
	if (settings.contains("geometry")) {
		restoreGeometry(settings.value("geometry").toByteArray());
	} else {
	}
	restoreState(settings.value("myWidget/windowState").toByteArray());

	frontScene->setSceneRect(0, 0, 400, 300);
	backScene->setSceneRect(0, 0, 400, 300);
    frontScene->addRect(0, 0, 400, 300);

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
	auto reply = QMessageBox::question(this, "確認", "終了しますか？");
	if (reply == QMessageBox::Yes) {
		QSettings settings;
		settings.setValue("geometry", saveGeometry());
		settings.setValue("windowState", saveState());
		QMainWindow::closeEvent(event);
	} else {
		event->ignore();
	}
}
