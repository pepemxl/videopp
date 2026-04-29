#include <QApplication>
#include <QMetaType>
#include <opencv2/opencv.hpp>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    qRegisterMetaType<cv::Mat>("cv::Mat");

    MainWindow w;
    w.resize(800, 600);
    w.show();

    return app.exec();
}