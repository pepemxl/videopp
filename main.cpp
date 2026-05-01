#include <QApplication>
#include <QCoreApplication>
#include <QMetaType>
#include <opencv2/opencv.hpp>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName("Pepe");
    QCoreApplication::setApplicationName("PepeGolfDiscAnalyzer");

    QApplication app(argc, argv);

    qRegisterMetaType<cv::Mat>("cv::Mat");

    MainWindow w;
    w.resize(1200, 900);
    w.show();

    return app.exec();
}