#include <QApplication>
#include <QCoreApplication>
#include <QIcon>
#include <QMetaType>
#include <opencv2/opencv.hpp>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName("Pepe");
    QCoreApplication::setApplicationName("PepeDGA");

    QApplication app(argc, argv);

    qRegisterMetaType<cv::Mat>("cv::Mat");

    MainWindow w;
    w.resize(1920, 1080);
    w.setWindowIcon(QIcon(":/logo.png"));
    w.show();

    return app.exec();
}