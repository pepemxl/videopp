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
    void onProcessedFrame(const cv::Mat &frame);
    void onBrowseFile();
    void onStartFromFile();
    void onStartFromCamera();
    void onStop();

private:
    QLabel     *m_videoLabel;
    QPushButton *m_btnStartFile;
    QPushButton *m_btnStartCam;
    QPushButton *m_btnStop;
    VideoProcessor *m_processor;

    cv::Mat matToGrayscale(const cv::Mat &src);
};

#endif