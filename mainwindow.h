#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGraphicsScene>

#include <forward_list>

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

private slots:
    void on_actionBoardes_triggered();

private:
    Ui::MainWindow *ui;
    QGraphicsScene *frontScene;
    QGraphicsScene *backScene;
    std::forward_list<PlacedComponent *> dataList;
};
#endif // MAINWINDOW_H
