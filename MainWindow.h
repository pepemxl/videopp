#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QAction>
#include <QImage>
#include <QStringList>
#include <QVector>
#include "VideoProcessor.h"

struct Marker
{
    QString type;     // "player" or "disc"
    int     x{0};     // image-space coordinates (independent of zoom/scale)
    int     y{0};
    double  timeSec{0.0};
};

class QComboBox;
class QMenu;
class QScrollArea;
class QSlider;

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
    void onRestart();
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
    void onToggleRecord(bool checked);
    void onRecordingChanged(bool recording);
    void onDurationChanged(double seconds);
    void onPositionChanged(double seconds);
    void onSliderMoved(int value);
    void onSliderReleased();
    void onAddPlayerMarker(bool checked);
    void onAddDiscMarker(bool checked);
    void onSaveMarkers();
    void onLoadMarkers();
    void onClearMarkers();
    void onToggleMarkersVisible(bool checked);

private:
    void updatePlaybackControls();
    void updateSpeedLabel();
    void updateTimeLabel();
    void renderFrame();
    void startFile(const QString &path, double startSec = 0.0);
    void addToRecent(const QString &path);
    void loadRecent();
    void saveRecent();
    void rebuildRecentMenu();
    QString nextRecordingPath() const;
    QString nextMarkersPath() const;
    QString markersBaseDirForCurrentVideo() const;
    void zoomBy(double factor, QPointF viewportFocus);
    void drawMarkersOnPixmap(QPixmap &pix) const;
    void placeMarkerAt(int imgX, int imgY);
    void cancelMarkerPlacement();
    bool saveMarkersToFile(const QString &path);
    bool loadMarkersFromFile(const QString &path);
    static QString formatTime(double seconds);

    QLabel       *m_videoLabel;
    QScrollArea  *m_scrollArea;
    QSlider      *m_seekSlider;
    QLabel       *m_timeLabel;
    QPushButton  *m_btnStartFile;
    QPushButton  *m_btnStartCam;
    QPushButton  *m_btnStop;
    QPushButton  *m_btnRestart;
    QPushButton  *m_btnPlayPause;
    QPushButton  *m_btnBackward;
    QPushButton  *m_btnForward;
    QPushButton  *m_btnRecord;
    QComboBox    *m_formatCombo;
    QPushButton  *m_btnAddPlayer;
    QPushButton  *m_btnAddDisc;
    QPushButton  *m_btnSaveMarkers;
    QLabel       *m_speedLabel;

    QAction      *m_actGrayscale;
    QAction      *m_actBlur;
    QAction      *m_actRecord;
    QAction      *m_actShowMarkers;
    QMenu        *m_recentMenu;
    QAction      *m_clearRecentAction;

    VideoProcessor *m_processor;

    QImage   m_currentFrame;
    double   m_zoom{1.0};
    QString  m_lastSource;
    double   m_lastPosSec{0.0};
    bool     m_isFileSource{false};
    double   m_durationSec{0.0};
    double   m_positionSec{0.0};
    bool     m_userScrubbing{false};
    QStringList m_recentVideos;

    enum class PendingMarker { None, Player, Disc };
    PendingMarker  m_pendingMarker{PendingMarker::None};
    QVector<Marker> m_markers;
    bool           m_markersVisible{true};
};

#endif
