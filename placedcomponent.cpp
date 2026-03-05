#include "placedcomponent.h"

#include <QGraphicsItem>
#include <QPainter>

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
	FrontItem(std::shared_ptr<Component> comp)
		: Item()
		, comp(comp)
	{}
	~FrontItem() {}

private:
	std::shared_ptr<Component> comp;
};
class BackItem : public Item
{
public:
	BackItem(std::shared_ptr<Component> comp)
		: Item()
		, comp(comp)
	{}
	~BackItem() {}

private:
	std::shared_ptr<Component> comp;
};

PlacedComponent::PlacedComponent(QGraphicsScene *frontScene,
								 QGraphicsScene *backScene,
								 std::shared_ptr<Component> comp)
	: front(new FrontItem(comp))
	, back(new BackItem(comp))
	, frontScene(frontScene)
	, backScene(backScene)
	, comp(comp)
{
	frontScene->addItem(front);
	backScene->addItem(back);
}
PlacedComponent::~PlacedComponent()
{
	frontScene->removeItem(front);
	backScene->removeItem(back);
	delete front;
	delete back;
}
