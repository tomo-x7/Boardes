#include "mainwindow.h"

#include <QGraphicsItem>

#include "./about.h"
#include "./ui_mainwindow.h"

class Item : public QGraphicsItem
{
public:
    Item()
        : QGraphicsItem()
    {}
    virtual ~Item() {}
    QRectF boundingRect() const override { return QRectF(0, 0, 50, 10); }
    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = nullptr) override
    {
        painter->setPen(QPen());
        painter->drawRect(0, 0, 50, 10);
    }
};
class FrontItem : public Item
{
public:
    FrontItem()
        : Item()
    {}
    ~FrontItem() {}
};
class BackItem : public Item
{
public:
    BackItem()
        : Item()
    {}
    ~BackItem() {}
};

class PlacedComponent
{
public:
    PlacedComponent(QGraphicsScene *frontScene, QGraphicsScene *backScene)
        : front(new FrontItem())
        , back(new BackItem())
        , frontScene(frontScene)
        , backScene(backScene)
    {
        frontScene->addItem(front);
        backScene->addItem(back);
    }
    ~PlacedComponent()
    {
        frontScene->removeItem(front);
        backScene->removeItem(back);
        delete front;
        delete back;
    }

private:
    FrontItem *front;
    BackItem *back;
    QGraphicsScene *frontScene;
    QGraphicsScene *backScene;
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , frontScene(new QGraphicsScene(this))
    , backScene(new QGraphicsScene(this))
    , dataList()
{
    ui->setupUi(this);
    frontScene->setSceneRect(0, 0, 400, 300);
    backScene->setSceneRect(0, 0, 400, 300);
    frontScene->addRect(0, 0, 400, 300);

    auto c1 = new PlacedComponent(frontScene, backScene);
    dataList.push_front(c1);

    ui->frontView->setScene(frontScene);
    ui->backView->setScene(backScene);
}

MainWindow::~MainWindow()
{
    delete ui;
    for (auto item : dataList) {
        delete item;
    }
    dataList.clear();
}

void MainWindow::on_actionBoardes_triggered()
{
    auto about = About();
    about.exec();
}
