#pragma once

#include <QGraphicsScene>
#include <QMainWindow>
#include "libmanager.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class PlacedComponent;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
	void closeEvent(QCloseEvent *event) override;

private slots:
	void on_aboutAction_triggered();

	void on_itemSelectType_currentIndexChanged(int index);

private:
	Ui::MainWindow *ui;
    QGraphicsScene *frontScene;
	QGraphicsScene *backScene;
	LibManager libManager;
};
