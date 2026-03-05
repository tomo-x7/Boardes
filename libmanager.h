#pragma once

#include <QPixmap>
#include <string>

// コンポーネントはライブラリに含まれる。そのライブラリを管理するクラスであり、全てのコンポーネントを管理。
class LibManager
{
public:
	LibManager();
	~LibManager();
};

class Component
{
public:
	Component(std::string path);
	~Component();
	QPixmap pixmap;
	std::string name;

private:
	int height, width;
	unsigned char* data;
};
