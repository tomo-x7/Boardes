#pragma once

#include <QPixmap>
#include "util.h"
#include <map>
#include <string>
// コンポーネントはライブラリに含まれる。そのライブラリを管理するクラスであり、全てのコンポーネントを管理。

class Component
{
public:
	Component(std::string path, std::string name);
	~Component();
	QPixmap pixmap;
	std::string name;
	std::string toJson();
	int height, width;

	MOVEONLY(Component)

private:
	unsigned char* data;
};
class Category
{
public:
	Category();
	~Category();
	std::string toJson();
	std::map<std::string, std::shared_ptr<Component>> components;

	MOVEONLY(Category)
};
class Library
{
public:
	Library();
	~Library();
	std::string toJson();
	std::map<std::string, Category> categories;

	MOVEONLY(Library)
};

class LibManager
{
public:
	LibManager();
	~LibManager();
	std::map<std::string, Library> libraries;

	MOVEONLY(LibManager)
private:
};
