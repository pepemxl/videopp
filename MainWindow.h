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

struct ShotMetadata
{
    QString playerName;
    QString pdgaNumber;
    QString courseName;
    QString courseLayout;
    QString hole;
};

class QComboBox;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QMenu;
class QScrollArea;
class QSlider;
class QSpinBox;

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
    void onMarkerFileActivated(QListWidgetItem *item);
    void onMarkerListActivated(QListWidgetItem *item);
    void onDeleteSelectedMarker();
    void onToggleSelectedMarkerType();
    void onMoveSelectedMarker(bool checked);
    void onFilterSelected(int filterType);

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
    void rebuildCurrentFrame();
    QWidget *buildLeftSidebar();
    QWidget *buildRightSidebar();
    void refreshMarkerFileList();
    void refreshMarkerList();
    int  selectedMarkerIndex() const;
    ShotMetadata currentMetadataFromForm() const;
    void applyMetadataToForm(const ShotMetadata &meta);
    void clearMetadataForm();
    void clearTelemetry();
public:
    // Telemetry setters — wire these up when an analysis pipeline lands.
    void setSpinRpm(double rpm);
    void setNoseDeg(double degrees);
    void setApexMeters(double meters);
    void setDistanceMeters(double meters);
    void setPoseJointAngles(const QVector<QPair<QString, double>> &angles);
private:
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
    QListWidget  *m_markerFileList;
    QListWidget  *m_markerList;            // current session markers
    QPushButton  *m_btnDeleteMarker;
    QPushButton  *m_btnToggleType;
    QPushButton  *m_btnMoveMarker;
    QLineEdit    *m_edPlayerName;
    QLineEdit    *m_edPdgaNumber;
    QLineEdit    *m_edCourseName;
    QLineEdit    *m_edCourseLayout;
    QSpinBox     *m_spHole;
    QLabel       *m_lblSpin;
    QLabel       *m_lblNose;
    QLabel       *m_lblApex;
    QLabel       *m_lblDistance;
    QListWidget  *m_poseJointList;
    QLabel       *m_speedLabel;

    QAction      *m_actRecord;
    QAction      *m_actShowMarkers;
    QMenu        *m_recentMenu;
    QAction      *m_clearRecentAction;

    VideoProcessor *m_processor;

    cv::Mat  m_rawFrame;        // last raw frame as received from the worker
    int      m_filter{0};       // Filters::None
    QImage   m_currentFrame;    // post-filter, ready for display
    double   m_zoom{1.0};
    QString  m_lastSource;
    double   m_lastPosSec{0.0};
    bool     m_isFileSource{false};
    double   m_durationSec{0.0};
    double   m_positionSec{0.0};
    bool     m_userScrubbing{false};
    QStringList m_recentVideos;

    enum class PendingMarker { None, Player, Disc, MoveExisting };
    PendingMarker  m_pendingMarker{PendingMarker::None};
    int            m_pendingMoveIndex{-1};
    QVector<Marker> m_markers;
    bool           m_markersVisible{true};
};

#endif
