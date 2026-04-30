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
    void onPlayPause();
    void onSeekForward();
    void onSeekBackward();
    void onProcessorFinished();
    void onError(const QString &message);

private:
    void updatePlaybackControls();

    QLabel      *m_videoLabel;
    QPushButton *m_btnStartFile;
    QPushButton *m_btnStartCam;
    QPushButton *m_btnStop;
    QPushButton *m_btnPlayPause;
    QPushButton *m_btnBackward;
    QPushButton *m_btnForward;
    VideoProcessor *m_processor;
    bool m_isFileSource{false};
};

#endif