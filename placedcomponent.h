#pragma once

#include <QGraphicsScene>
#include <QMainWindow>
#include "libmanager.h"
#include "util.h"

class FrontItem;
class BackItem;

// 配置されたコンポーネント。コンポーネントへの参照を利用して描画などを行う。
class PlacedComponent
{
public:
	PlacedComponent(QGraphicsScene *frontScene,
					QGraphicsScene *backScene,
					std::shared_ptr<Component> comp);
	~PlacedComponent();

	MOVEONLY(PlacedComponent)
private:
	FrontItem *front;
	BackItem *back;
	QGraphicsScene *frontScene;
	QGraphicsScene *backScene;
	std::shared_ptr<Component> comp;
};
