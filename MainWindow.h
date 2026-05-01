#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QAction>
#include <QImage>
#include <QStringList>
#include "VideoProcessor.h"

class QMenu;
class QScrollArea;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

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
    void onSpeedUp();
    void onSpeedDown();
    void onResetSpeed();
    void onZoomIn();
    void onZoomOut();
    void onResetZoom();
    void onOpenRecentTriggered();
    void onClearRecent();

private:
    void updatePlaybackControls();
    void updateSpeedLabel();
    void renderFrame();
    void startFile(const QString &path, double startSec = 0.0);
    void addToRecent(const QString &path);
    void loadRecent();
    void saveRecent();
    void rebuildRecentMenu();

    QLabel       *m_videoLabel;
    QScrollArea  *m_scrollArea;
    QPushButton  *m_btnStartFile;
    QPushButton  *m_btnStartCam;
    QPushButton  *m_btnStop;
    QPushButton  *m_btnPlayPause;
    QPushButton  *m_btnBackward;
    QPushButton  *m_btnForward;
    QLabel       *m_speedLabel;

    QAction      *m_actGrayscale;
    QAction      *m_actBlur;
    QMenu        *m_recentMenu;
    QAction      *m_clearRecentAction;

    VideoProcessor *m_processor;

    QImage   m_currentFrame;
    double   m_zoom{1.0};
    QString  m_lastSource;
    double   m_lastPosSec{0.0};
    bool     m_isFileSource{false};
    QStringList m_recentVideos;
};

#endif
