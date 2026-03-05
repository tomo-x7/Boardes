#include "mainwindow.h"

#include <QApplication>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include "stb_image.h"

int main(int argc, char *argv[])
{
	QCoreApplication::setOrganizationName("tomo-x");
	QCoreApplication::setOrganizationDomain("tomo-x.win");
	QCoreApplication::setApplicationName("Boardes");

	QApplication a(argc, argv);
	MainWindow w;
	w.show();
	return a.exec();
}
