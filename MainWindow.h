#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include "VideoProcessor.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onFrameReady(const cv::Mat &frame);
    void onStartFromFile();
    void onStartFromCamera();
    void onStop();
    void onError(const QString &message);

private:
    QLabel     *m_videoLabel;
    QPushButton *m_btnStartFile;
    QPushButton *m_btnStartCam;
    QPushButton *m_btnStop;
    VideoProcessor *m_processor;
};

#endif