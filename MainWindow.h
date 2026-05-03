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

class QActionGroup;
class QComboBox;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QMenu;
class QPlainTextEdit;
class QProcess;
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
    void onGenerateDiscConfig();
    void onGeneratePlayerConfig();
    void onGenerateHighlighterConfig();
    void onRunDiscTracker();
    void onRunPlayerTracker();
    void onRunPlayerHighlighter();
    void onLoadStats();
    void onSubserviceStdout();
    void onSubserviceStderr();
    void onSubserviceFinished(int exitCode);

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
    void     buildSubservicesGroup(class QVBoxLayout *parent);

    // Marker dirty-state tracking + autosave-before-action helper.
    // Returns the path of the (possibly auto-)saved markers file, or
    // QString() if there's nothing to save / save failed.
    QString  ensureMarkersSaved();

    // Config generators — each writes a fresh YAML next to the markers
    // (LOCAL_DATA/configs/<video>/) and returns the absolute path.
    QString  writeDiscTrackerConfig(const QString &markersPath);
    QString  writePlayerTrackerConfig(const QString &markersPath);
    QString  writePlayerHighlighterConfig(const QString &playerCsvPath);

    // Locate a subservice executable across known build directories.
    // `name` is the bare exe name without extension (e.g. "disc_tracker").
    QString  findSubserviceExe(const QString &name) const;

    // Find the most-recent player_tracker CSV for the current video, by
    // mtime under player_tracker/out/<videoStem>_player_*.csv.
    QString  findLatestPlayerCsv() const;

    // Spawn a subservice as a QProcess, streaming stdout/stderr to the log.
    void     runSubservice(const QString &serviceName,
                           const QString &exePath,
                           const QString &configPath);
    void     appendLog(const QString &line);

    // Read a player_tracker CSV, compute six priority joint angles
    // (left/right elbow, shoulder, knee), aggregate over frames, and
    // populate the existing Pose Joint Angles list with min/mean/max.
    void     loadJointAnglesFromCsv(const QString &csvPath);
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
    QActionGroup *m_filterGroup{nullptr};
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
    bool           m_markersDirty{false};

    // Subservices sidebar — buttons + log pane + last-generated config paths.
    QPushButton    *m_btnGenDiscCfg{nullptr};
    QPushButton    *m_btnGenPlayerCfg{nullptr};
    QPushButton    *m_btnGenHighlighterCfg{nullptr};
    QPushButton    *m_btnRunDisc{nullptr};
    QPushButton    *m_btnRunPlayer{nullptr};
    QPushButton    *m_btnRunHighlighter{nullptr};
    QPushButton    *m_btnLoadStats{nullptr};
    QPlainTextEdit *m_subserviceLog{nullptr};
    QProcess       *m_subserviceProc{nullptr};
    QString         m_runningServiceName;
    QString         m_lastDiscCfg;
    QString         m_lastPlayerCfg;
    QString         m_lastHighlighterCfg;
};

#endif
