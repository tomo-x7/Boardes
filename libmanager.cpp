#include "libmanager.h"

#include <QStandardPaths>
#include "stb_image.h"

LibManager::LibManager()
{
	auto path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

Component::Component(std::string path)
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
}
Component::~Component() {}
