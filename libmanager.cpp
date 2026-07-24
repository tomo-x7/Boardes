#include "libmanager.h"

#include <QStandardPaths>
#include "stb_image.h"
#include <memory>

Category::Category()
	: components()
{
	auto shared = std::make_shared<Component>("/home/tomo/Documents/pass/parts/R/R-2.bmp",
											  "testitem");
	components.emplace("testitem", shared);
};
Category::~Category() {};

Library::Library()
	: categories()
{
	categories.emplace("testcat", Category());
};
Library::~Library() {};

LibManager::LibManager()
	: libraries()
{
	auto path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
	libraries.emplace("testlib", Library());
}
LibManager::~LibManager() {}

Component::Component(std::string path, std::string name)
{
	int x, y, n;
	unsigned char *rawdata = stbi_load(path.c_str(), &x, &y, &n, 4);
	if (!rawdata) {
		const char *reason = stbi_failure_reason();
		throw std::runtime_error("画像" + path + "の読み込みに失敗しました\n" + std::string(reason));
	}
	this->data = rawdata;
	this->height = y;
	this->width = x;
	QImage qi = QImage(rawdata, x, y, QImage::Format_RGBA8888);
	this->pixmap = QPixmap::fromImage(qi);
	this->name = name;
}
Component::~Component()
{
	stbi_image_free(this->data);
}
