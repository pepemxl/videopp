#include "MainWindow.h"
#include "Filters.h"
#include "IconManager.h"
#include <cmath>
#include <QActionGroup>
#include <QComboBox>
#include <QCoreApplication>
#include <QCursor>
#include <QDateTime>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPlainTextEdit>
#include <QProcess>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QApplication>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>
#include <QTextStream>
#include <QTime>
#include <QVBoxLayout>
#include <QWidget>
#include <QImage>
#include <QPixmap>
#include <QtMath>
#include <QHash>
#include <stdexcept>
#include <opencv2/core/persistence.hpp>
#include <QWheelEvent>

namespace {
constexpr double kSeekStepSeconds = 5.0;
constexpr double kSpeedStep       = 1.25;   // multiplicative
constexpr double kSpeedMin        = 0.25;
constexpr double kSpeedMax        = 4.0;
constexpr double kZoomStep        = 1.15;   // multiplicative
constexpr double kZoomMin         = 1.0;    // fit-to-window is the floor
constexpr double kZoomMax         = 6.0;
constexpr int    kMaxRecent       = 10;
constexpr int    kSliderMaxMs     = 1'000'000'000;  // ~11.5 days, safe upper bound
constexpr double kMarkerWindowSec = .5;             // visible for this long after placement
const QString kRecentKey      = QStringLiteral("recentVideos");
const QString kLastOpenDirKey = QStringLiteral("lastOpenDir");
const QString kThemeKey       = QStringLiteral("theme");

// Captured the first time applyTheme() runs, so "System" can restore whatever
// QApplication was constructed with (e.g. windows11/windowsvista on Win11).
QString g_defaultStyleName;
QPalette g_defaultPalette;
bool     g_defaultsCaptured = false;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_processor(new VideoProcessor(this))
{
    setWindowTitle(tr("Pepe DGA"));

    m_videoLabel = new QLabel(tr("Video display"));
    m_videoLabel->setAlignment(Qt::AlignCenter);
    m_videoLabel->setBackgroundRole(QPalette::Dark);

    m_scrollArea = new QScrollArea;
    m_scrollArea->setWidget(m_videoLabel);
    // widgetResizable=false: the label sizes itself to the pixmap so scrollbars
    // appear when zoomed in. With =true the label is clamped to viewport size
    // and any pixmap larger than the viewport is clipped (and markers go with it).
    m_scrollArea->setWidgetResizable(false);
    m_scrollArea->setAlignment(Qt::AlignCenter);
    m_scrollArea->setMinimumSize(640, 480);
    m_scrollArea->viewport()->installEventFilter(this);
    m_scrollArea->installEventFilter(this);

    // Create Buttons
    //m_btnStartFile = new QPushButton(tr("Start from file"));
    m_btnStartFile = new QPushButton(this);
    m_btnStartFile->setIcon(IconManager::instance().getIcon("open"));
    m_btnStartFile->setIconSize(QSize(48, 48));
    m_btnStartFile->setFixedSize(70, 70);
    m_btnStartFile->setToolTip("Haz clic para abrir archivo");
    m_btnStartFile->setFocusPolicy(Qt::NoFocus);
    //m_btnStartCam  = new QPushButton(tr("Start from camera"));
    m_btnStartCam = new QPushButton(this);
    m_btnStartCam->setIcon(IconManager::instance().getIcon("camera"));
    m_btnStartCam->setIconSize(QSize(48, 48));
    m_btnStartCam->setFixedSize(70, 70);
    m_btnStartCam->setToolTip("Haz clic para capturar desde camara");
    m_btnStartCam->setFocusPolicy(Qt::NoFocus);
    //m_btnStop      = new QPushButton(tr("Stop"));
    m_btnStop = new QPushButton(this);
    m_btnStop->setIcon(IconManager::instance().getIcon("stop"));
    m_btnStop->setIconSize(QSize(48, 48));
    m_btnStop->setFixedSize(70, 70);
    m_btnStop->setToolTip("Haz clic para parar");
    m_btnStop->setFocusPolicy(Qt::NoFocus);
    //m_btnRestart   = new QPushButton(tr("Restart"));
    m_btnRestart = new QPushButton(this);
    m_btnRestart->setIcon(IconManager::instance().getIcon("repeat"));
    m_btnRestart->setIconSize(QSize(48, 48));
    m_btnRestart->setFixedSize(70, 70);
    m_btnRestart->setToolTip("Haz clic para restart");
    m_btnRestart->setFocusPolicy(Qt::NoFocus);
    //m_btnBackward  = new QPushButton(tr("<< -5s"));
    m_btnBackward = new QPushButton(this);
    m_btnBackward->setIcon(IconManager::instance().getIcon("backward"));
    m_btnBackward->setIconSize(QSize(48, 48));
    m_btnBackward->setFixedSize(70, 70);
    m_btnBackward->setToolTip("Haz clic para -5s rewind");
    m_btnBackward->setFocusPolicy(Qt::NoFocus);
    //m_btnPlayPause = new QPushButton(tr("Pause"));
    m_btnPlayPause = new QPushButton(this);
    //m_btnPlayPause->setIcon(QIcon(":/iconos/icono_play.png"));
    m_btnPlayPause->setIcon(IconManager::instance().getIcon("pause"));
    m_btnPlayPause->setIconSize(QSize(48, 48));
    m_btnPlayPause->setFixedSize(70, 70);
    m_btnPlayPause->setToolTip("Haz clic para reproducir");
    m_btnPlayPause->setFocusPolicy(Qt::NoFocus);

    //m_btnForward   = new QPushButton(tr("+5s >>"));
    m_btnForward = new QPushButton(this);
    m_btnForward->setIcon(IconManager::instance().getIcon("forward"));
    m_btnForward->setIconSize(QSize(48, 48));
    m_btnForward->setFixedSize(70, 70);
    m_btnForward->setToolTip("Haz clic para +5s forward");
    m_btnForward->setFocusPolicy(Qt::NoFocus);
    //m_btnRecord    = new QPushButton(tr("Record"));
    m_btnRecord = new QPushButton(this);
    m_btnRecord->setIcon(IconManager::instance().getIcon("record"));
    m_btnRecord->setIconSize(QSize(48, 48));
    m_btnRecord->setFixedSize(70, 70);
    m_btnRecord->setToolTip("Haz clic para grabar fragmento");
    m_btnRecord->setFocusPolicy(Qt::NoFocus);
    m_btnRecord->setCheckable(true);
    m_formatCombo  = new QComboBox;
    m_formatCombo->addItems({QStringLiteral("mp4"),
                             QStringLiteral("avi"),
                             QStringLiteral("mkv")});
    m_formatCombo->setToolTip(tr("Recording container format"));

    //m_btnAddPlayer    = new QPushButton(tr("Add Player Marker"));
    m_btnAddPlayer = new QPushButton(this);
    m_btnAddPlayer->setIcon(IconManager::instance().getIcon("player"));
    m_btnAddPlayer->setIconSize(QSize(48, 48));
    m_btnAddPlayer->setFixedSize(70, 70);
    m_btnAddPlayer->setToolTip("Haz clic para agregar player markers");
    m_btnAddPlayer->setFocusPolicy(Qt::NoFocus);
    m_btnAddPlayer->setCheckable(true);
    //m_btnAddDisc      = new QPushButton(tr("Add Disc Marker"));
    m_btnAddDisc = new QPushButton(this);
    m_btnAddDisc->setIcon(IconManager::instance().getIcon("disc"));
    m_btnAddDisc->setIconSize(QSize(48, 48));
    m_btnAddDisc->setFixedSize(70, 70);
    m_btnAddDisc->setToolTip("Haz clic para agregar disc markers");
    m_btnAddDisc->setFocusPolicy(Qt::NoFocus);
    m_btnAddDisc->setCheckable(true);
    //m_btnSaveMarkers  = new QPushButton(tr("Save Markers"));
    m_btnSaveMarkers = new QPushButton(this);
    m_btnSaveMarkers->setIcon(IconManager::instance().getIcon("save"));
    m_btnSaveMarkers->setIconSize(QSize(48, 48));
    m_btnSaveMarkers->setFixedSize(70, 70);
    m_btnSaveMarkers->setFocusPolicy(Qt::NoFocus);
    m_btnSaveMarkers->setToolTip(tr("Save current markers to LOCAL_DATA/configs/<video>/"));

    m_speedLabel   = new QLabel(tr("Speed: 1.00x"));

    m_seekSlider = new QSlider(Qt::Horizontal);
    m_seekSlider->setRange(0, 0);
    m_seekSlider->setSingleStep(1000);
    m_seekSlider->setPageStep(5000);
    m_seekSlider->setEnabled(false);
    m_timeLabel = new QLabel(QStringLiteral("00:00 / 00:00"));
    m_timeLabel->setMinimumWidth(120);
    m_timeLabel->setAlignment(Qt::AlignCenter);

    auto seekRow = new QHBoxLayout;
    seekRow->addWidget(m_seekSlider, 1);
    seekRow->addWidget(m_timeLabel);

    auto sourceRow = new QHBoxLayout;
    sourceRow->addStretch();
    sourceRow->addWidget(m_btnStartCam);
    sourceRow->addWidget(m_btnStartFile);

    auto playbackRow = new QHBoxLayout;
    playbackRow->addStretch();
    playbackRow->addWidget(m_btnBackward);
    playbackRow->addWidget(m_btnPlayPause);
    playbackRow->addWidget(m_btnStop);
    playbackRow->addWidget(m_btnForward);
    playbackRow->addWidget(m_btnRestart);
    playbackRow->addStretch();
    playbackRow->addWidget(m_btnRecord);
    playbackRow->addWidget(m_formatCombo);
    playbackRow->addWidget(m_speedLabel);

    auto centerLayout = new QVBoxLayout;
    centerLayout->addWidget(m_scrollArea, 1);
    centerLayout->addLayout(seekRow);
    centerLayout->addLayout(playbackRow);
    centerLayout->addLayout(sourceRow);

    auto centerWidget = new QWidget;
    centerWidget->setLayout(centerLayout);

    QWidget *leftSidebar  = buildLeftSidebar();
    QWidget *rightSidebar = buildRightSidebar();

    auto splitter = new QSplitter(Qt::Horizontal);
    splitter->addWidget(leftSidebar);
    splitter->addWidget(centerWidget);
    splitter->addWidget(rightSidebar);
    splitter->setCollapsible(0, true);
    splitter->setCollapsible(1, false);
    splitter->setCollapsible(2, true);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);
    splitter->setSizes({260, 800, 260});

    setCentralWidget(splitter);

    // ----- File menu -----
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    QAction *actOpenFile = fileMenu->addAction(tr("&Open Video..."));
    actOpenFile->setShortcut(QKeySequence::Open);
    QAction *actOpenCam = fileMenu->addAction(tr("Open &Camera"));
    fileMenu->addSeparator();
    m_recentMenu = fileMenu->addMenu(tr("&Recent Videos"));
    fileMenu->addSeparator();
    QAction *actExit = fileMenu->addAction(tr("E&xit"));
    actExit->setShortcut(QKeySequence::Quit);

    connect(actOpenFile, &QAction::triggered, this, &MainWindow::onStartFromFile);
    connect(actOpenCam,  &QAction::triggered, this, &MainWindow::onStartFromCamera);
    connect(actExit,     &QAction::triggered, this, &QWidget::close);

    // ----- Playback menu -----
    QMenu *playbackMenu = menuBar()->addMenu(tr("&Playback"));
    QAction *actPlayPause = playbackMenu->addAction(tr("&Play/Pause"));
    actPlayPause->setShortcut(Qt::Key_Space);
    QAction *actStop = playbackMenu->addAction(tr("&Stop"));
    QAction *actRestart = playbackMenu->addAction(tr("Re&start"));
    actRestart->setShortcut(QKeySequence(tr("Ctrl+R")));
    playbackMenu->addSeparator();
    QAction *actSpeedUp = playbackMenu->addAction(tr("Speed &Up"));
    actSpeedUp->setShortcut(QKeySequence(tr("Ctrl+Right")));
    QAction *actSpeedDown = playbackMenu->addAction(tr("Speed &Down"));
    actSpeedDown->setShortcut(QKeySequence(tr("Ctrl+Left")));
    QAction *actResetSpeed = playbackMenu->addAction(tr("&Reset Speed"));
    actResetSpeed->setShortcut(QKeySequence(tr("Ctrl+0")));
    playbackMenu->addSeparator();
    m_actRecord = playbackMenu->addAction(tr("Re&cord..."));
    m_actRecord->setCheckable(true);
    m_actRecord->setShortcut(QKeySequence(tr("Ctrl+Shift+R")));

    connect(actPlayPause,  &QAction::triggered, this, &MainWindow::onPlayPause);
    connect(actStop,       &QAction::triggered, this, &MainWindow::onStop);
    connect(actRestart,    &QAction::triggered, this, &MainWindow::onRestart);
    connect(actSpeedUp,    &QAction::triggered, this, &MainWindow::onSpeedUp);
    connect(actSpeedDown,  &QAction::triggered, this, &MainWindow::onSpeedDown);
    connect(actResetSpeed, &QAction::triggered, this, &MainWindow::onResetSpeed);
    connect(m_actRecord,   &QAction::toggled,   this, &MainWindow::onToggleRecord);

    // ----- View menu -----
    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    QAction *actZoomIn = viewMenu->addAction(tr("Zoom &In"));
    actZoomIn->setShortcut(QKeySequence::ZoomIn);
    QAction *actZoomOut = viewMenu->addAction(tr("Zoom &Out"));
    actZoomOut->setShortcut(QKeySequence::ZoomOut);
    QAction *actZoomReset = viewMenu->addAction(tr("&Reset Zoom"));
    actZoomReset->setShortcut(QKeySequence(tr("Ctrl+1")));

    connect(actZoomIn,    &QAction::triggered, this, &MainWindow::onZoomIn);
    connect(actZoomOut,   &QAction::triggered, this, &MainWindow::onZoomOut);
    connect(actZoomReset, &QAction::triggered, this, &MainWindow::onResetZoom);

    viewMenu->addSeparator();
    QMenu *themeMenu = viewMenu->addMenu(tr("&Theme"));
    m_themeGroup = new QActionGroup(this);
    m_themeGroup->setExclusive(true);
    struct ThemeEntry { Theme id; const char *label; };
    const ThemeEntry kThemes[] = {
        { Theme::System,         QT_TR_NOOP("&System Default") },
        { Theme::Dark,           QT_TR_NOOP("&Dark") },
        { Theme::SolarizedLight, QT_TR_NOOP("Solarized &Light") },
    };
    const Theme savedTheme = loadSavedTheme();
    for (const auto &t : kThemes) {
        QAction *act = themeMenu->addAction(tr(t.label));
        act->setCheckable(true);
        act->setData(int(t.id));
        if (t.id == savedTheme) act->setChecked(true);
        m_themeGroup->addAction(act);
    }
    connect(m_themeGroup, &QActionGroup::triggered,
            this, &MainWindow::onThemeSelected);

    // ----- Markers menu -----
    QMenu *markersMenu = menuBar()->addMenu(tr("&Markers"));
    QAction *actAddPlayerM = markersMenu->addAction(tr("Add &Player Marker"));
    actAddPlayerM->setCheckable(true);
    QAction *actAddDiscM   = markersMenu->addAction(tr("Add &Disc Marker"));
    actAddDiscM->setCheckable(true);
    markersMenu->addSeparator();
    QAction *actSaveM = markersMenu->addAction(tr("&Save Markers"));
    QAction *actLoadM = markersMenu->addAction(tr("&Load Markers..."));
    QAction *actClearM = markersMenu->addAction(tr("&Clear Markers"));
    markersMenu->addSeparator();
    m_actShowMarkers = markersMenu->addAction(tr("Show &Markers"));
    m_actShowMarkers->setCheckable(true);
    m_actShowMarkers->setChecked(true);

    // Keep menu actions and their button counterparts in sync.
    connect(actAddPlayerM, &QAction::toggled, m_btnAddPlayer, &QPushButton::setChecked);
    connect(m_btnAddPlayer, &QPushButton::toggled, actAddPlayerM, &QAction::setChecked);
    connect(actAddDiscM,   &QAction::toggled, m_btnAddDisc,   &QPushButton::setChecked);
    connect(m_btnAddDisc,   &QPushButton::toggled, actAddDiscM, &QAction::setChecked);
    connect(actSaveM,  &QAction::triggered, this, &MainWindow::onSaveMarkers);
    connect(actLoadM,  &QAction::triggered, this, &MainWindow::onLoadMarkers);
    connect(actClearM, &QAction::triggered, this, &MainWindow::onClearMarkers);
    connect(m_actShowMarkers, &QAction::toggled,
            this, &MainWindow::onToggleMarkersVisible);

    // ----- Filters menu -----
    QMenu *filtersMenu = menuBar()->addMenu(tr("F&ilters"));
    m_filterGroup = new QActionGroup(this);
    m_filterGroup->setExclusive(true);
    for (int t = Filters::None; t < Filters::Count; ++t) {
        QAction *act = filtersMenu->addAction(Filters::displayName(t));
        act->setCheckable(true);
        act->setData(t);
        if (t == Filters::None) act->setChecked(true);
        m_filterGroup->addAction(act);
    }
    connect(m_filterGroup, &QActionGroup::triggered, this,
            [this](QAction *a) { onFilterSelected(a->data().toInt()); });

    // ----- Buttons -----
    connect(m_btnStartFile, &QPushButton::clicked, this, &MainWindow::onStartFromFile);
    connect(m_btnStartCam,  &QPushButton::clicked, this, &MainWindow::onStartFromCamera);
    connect(m_btnStop,      &QPushButton::clicked, this, &MainWindow::onStop);
    connect(m_btnRestart,   &QPushButton::clicked, this, &MainWindow::onRestart);
    connect(m_btnPlayPause, &QPushButton::clicked, this, &MainWindow::onPlayPause);
    connect(m_btnForward,   &QPushButton::clicked, this, &MainWindow::onSeekForward);
    connect(m_btnBackward,  &QPushButton::clicked, this, &MainWindow::onSeekBackward);
    connect(m_btnRecord,    &QPushButton::toggled, this, &MainWindow::onToggleRecord);
    connect(m_btnAddPlayer, &QPushButton::toggled, this, &MainWindow::onAddPlayerMarker);
    connect(m_btnAddDisc,   &QPushButton::toggled, this, &MainWindow::onAddDiscMarker);
    connect(m_btnSaveMarkers, &QPushButton::clicked, this, &MainWindow::onSaveMarkers);

    // Capture clicks on the video label for marker placement.
    m_videoLabel->setMouseTracking(false);
    m_videoLabel->installEventFilter(this);

    // ----- Seek slider -----
    connect(m_seekSlider, &QSlider::sliderPressed, this,
            [this]() { m_userScrubbing = true; });
    connect(m_seekSlider, &QSlider::sliderMoved,    this, &MainWindow::onSliderMoved);
    connect(m_seekSlider, &QSlider::sliderReleased, this, &MainWindow::onSliderReleased);

    // ----- Processor signals -----
    connect(m_processor, &VideoProcessor::frameReady,
            this, &MainWindow::onFrameReady);
    connect(m_processor, &VideoProcessor::finished,
            this, &MainWindow::onProcessorFinished);
    connect(m_processor, &VideoProcessor::error,
            this, &MainWindow::onError);
    connect(m_processor, &VideoProcessor::durationChanged,
            this, &MainWindow::onDurationChanged);
    connect(m_processor, &VideoProcessor::positionChanged,
            this, &MainWindow::onPositionChanged);
    connect(m_processor, &VideoProcessor::recordingChanged,
            this, &MainWindow::onRecordingChanged);

    loadRecent();
    rebuildRecentMenu();
    updatePlaybackControls();
    updateSpeedLabel();
    updateTimeLabel();
}

MainWindow::~MainWindow()
{
    if (m_processor->isRunning()) {
        m_processor->stopProcessing();
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if ((watched == m_scrollArea || watched == m_scrollArea->viewport())
        && event->type() == QEvent::Wheel) {
        auto *we = static_cast<QWheelEvent *>(event);
        const int dy = we->angleDelta().y();
        if (dy != 0) {
            const double factor = (dy > 0) ? kZoomStep : (1.0 / kZoomStep);
            // we->position() is in the receiving widget's local coords.
            QPointF vpFocus = we->position();
            if (watched == m_scrollArea) {
                vpFocus = QPointF(m_scrollArea->viewport()->mapFrom(
                    m_scrollArea, we->position().toPoint()));
            }
            zoomBy(factor, vpFocus);
            return true;  // consume — don't scroll the area
        }
    }
    if (watched == m_videoLabel
        && event->type() == QEvent::MouseButtonPress
        && m_pendingMarker != PendingMarker::None) {
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::LeftButton && !m_currentFrame.isNull()) {
            const QPixmap pm = m_videoLabel->pixmap(Qt::ReturnByValue);
            if (!pm.isNull()) {
                const double sx = double(m_currentFrame.width())  / pm.width();
                const double sy = double(m_currentFrame.height()) / pm.height();
                const int imgX = static_cast<int>(me->position().x() * sx);
                const int imgY = static_cast<int>(me->position().y() * sy);
                if (imgX >= 0 && imgY >= 0
                    && imgX < m_currentFrame.width()
                    && imgY < m_currentFrame.height()) {
                    placeMarkerAt(imgX, imgY);
                    return true;
                }
            }
        } else if (me->button() == Qt::RightButton) {
            cancelMarkerPlacement();
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::onFrameReady(const cv::Mat &frame)
{
    if (frame.empty()) return;
    // Deep copy: the worker may overwrite its frame on the next iteration.
    m_rawFrame = frame.clone();
    rebuildCurrentFrame();
    renderFrame();
}

void MainWindow::rebuildCurrentFrame()
{
    if (m_rawFrame.empty()) {
        m_currentFrame = QImage();
        return;
    }
    cv::Mat display = m_rawFrame.clone();
    Filters::apply(m_filter, display);

    if (display.channels() == 3) {
        cv::Mat rgb;
        cv::cvtColor(display, rgb, cv::COLOR_BGR2RGB);
        m_currentFrame = QImage(rgb.data, rgb.cols, rgb.rows,
                                static_cast<int>(rgb.step),
                                QImage::Format_RGB888).copy();
    } else if (display.channels() == 1) {
        m_currentFrame = QImage(display.data, display.cols, display.rows,
                                static_cast<int>(display.step),
                                QImage::Format_Grayscale8).copy();
    } else {
        m_currentFrame = QImage();
    }
}

void MainWindow::onFilterSelected(int filterType)
{
    if (filterType < Filters::None || filterType >= Filters::Count) return;
    if (filterType == m_filter) return;

    // Warn before enabling computationally expensive filters.
    const bool isHeavy = (filterType == Filters::Vignette
                          || filterType == Filters::Stylization
                          || filterType == Filters::DetailEnhance);
    if (isHeavy) {
        const QString name = Filters::displayName(filterType);
        const auto btn = QMessageBox::warning(
            this,
            tr("Filter requires significant resources"),
            tr("The \"%1\" filter is computationally expensive and is "
               "intended for systems with GPU acceleration or substantial "
               "CPU headroom.\n\nApplying it during playback may cause "
               "stutter, lag, or dropped frames.\n\nContinue?").arg(name),
            QMessageBox::Yes | QMessageBox::Cancel,
            QMessageBox::Cancel);
        if (btn != QMessageBox::Yes) {
            // Revert the menu selection to the previously active filter.
            if (m_filterGroup) {
                for (QAction *a : m_filterGroup->actions()) {
                    if (a->data().toInt() == m_filter) {
                        QSignalBlocker b(m_filterGroup);
                        a->setChecked(true);
                        break;
                    }
                }
            }
            return;
        }
    }

    m_filter = filterType;
    // Worker only needs the filter for the recording path; it emits raw frames.
    m_processor->setFilter(filterType);
    // Re-apply on whatever frame is currently shown so the change is instant.
    rebuildCurrentFrame();
    renderFrame();
}

void MainWindow::renderFrame()
{
    if (m_currentFrame.isNull()) return;

    // maximumViewportSize is the viewport size assuming no scrollbars are
    // currently shown — stable across zoom changes (avoids the dance where
    // adding a scrollbar shrinks the viewport and shrinks the fit size).
    const QSize viewport = m_scrollArea->maximumViewportSize();
    QSize fit = m_currentFrame.size().scaled(viewport, Qt::KeepAspectRatio);
    QSize target = fit * m_zoom;
    if (target.width() < 1 || target.height() < 1) return;

    QPixmap pix = QPixmap::fromImage(m_currentFrame).scaled(
        target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (m_showDiscTrack)  drawDiscOverlayOnPixmap(pix);
    if (m_showSkeleton)   drawSkeletonOverlayOnPixmap(pix);
    if (m_markersVisible) {
        drawMarkersOnPixmap(pix);
    }
    m_videoLabel->setPixmap(pix);
    m_videoLabel->resize(pix.size());
}

void MainWindow::drawMarkersOnPixmap(QPixmap &pix) const
{
    if (m_markers.isEmpty() || pix.isNull() || m_currentFrame.isNull()) return;
    const double sx = double(pix.width())  / m_currentFrame.width();
    const double sy = double(pix.height()) / m_currentFrame.height();
    const double now = m_positionSec;

    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QFont f = painter.font();
    f.setBold(true);
    f.setPointSize(10);
    painter.setFont(f);

    for (const Marker &m : m_markers) {
        // Show only within [marker.time, marker.time + window].
        if (now < m.timeSec || now > m.timeSec + kMarkerWindowSec) continue;
        const QPointF c(m.x * sx, m.y * sy);
        const bool isPlayer = (m.type == QLatin1String("player"));
        const QColor color  = isPlayer ? QColor(255, 64, 64)
                                       : QColor( 64, 220, 255);
        painter.setPen(QPen(color, 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(c, 8.0, 8.0);
        painter.drawLine(c + QPointF(-12, 0), c + QPointF(12, 0));
        painter.drawLine(c + QPointF(0, -12), c + QPointF(0, 12));
        painter.setPen(color);
        painter.drawText(c + QPointF(11, -10),
                         isPlayer ? QStringLiteral("P") : QStringLiteral("D"));
    }
}

void MainWindow::startFile(const QString &path, double startSec)
{
    if (m_processor->isRunning()) {
        m_processor->stopProcessing();
    }
    // Markers are per-video — drop session markers when the source changes.
    if (m_lastSource != path) {
        m_markers.clear();
        refreshMarkerList();
        clearMetadataForm();
    }
    // If this is a user-initiated open (not a swap to a tracker output),
    // remember the new file as the "original" and reset overlay state.
    if (!m_swappingSource) {
        m_originalSourcePath = path;
        m_discTrack.clear();
        m_skelTrack.clear();
        if (m_btnShowDiscTrack && m_btnShowDiscTrack->isChecked()) {
            QSignalBlocker b(m_btnShowDiscTrack);
            m_btnShowDiscTrack->setChecked(false);
        }
        if (m_btnShowSkeleton && m_btnShowSkeleton->isChecked()) {
            QSignalBlocker b(m_btnShowSkeleton);
            m_btnShowSkeleton->setChecked(false);
        }
        m_showDiscTrack = false;
        m_showSkeleton  = false;
    }
    m_processor->setSource(path, VideoProcessor::FromFile);
    m_processor->setStartPositionSec(startSec);
    m_lastSource = path;
    m_lastPosSec = startSec;
    m_isFileSource = true;
    m_durationSec = 0.0;
    m_positionSec = startSec;
    //m_btnPlayPause->setText(tr("Pause"));
    m_btnPlayPause->setIcon(IconManager::instance().getIcon("pause"));
    {
        QSignalBlocker b(m_seekSlider);
        m_seekSlider->setRange(0, 0);
        m_seekSlider->setValue(0);
    }
    updateTimeLabel();
    m_processor->start();
    addToRecent(path);
    refreshMarkerFileList();
    updatePlaybackControls();
}

void MainWindow::onStartFromFile()
{
    QSettings settings;
    QString startDir = settings.value(kLastOpenDirKey).toString();
    if (startDir.isEmpty() || !QDir(startDir).exists()) {
        startDir = QDir::homePath();
    }
    const QString file = QFileDialog::getOpenFileName(
        this, tr("Open video file"), startDir,
        tr("Video (*.mp4 *.avi *.mkv *.mov *.webm)"));
    if (!file.isEmpty()) {
        settings.setValue(kLastOpenDirKey, QFileInfo(file).absolutePath());
        startFile(file, 0.0);
    }
}

void MainWindow::onStartFromCamera()
{
    if (m_processor->isRunning()) {
        m_processor->stopProcessing();
    }
    m_processor->setSource(QString(), VideoProcessor::FromCamera);
    m_isFileSource = false;
    m_lastSource.clear();
    m_lastPosSec = 0.0;
    m_durationSec = 0.0;
    m_positionSec = 0.0;
    {
        QSignalBlocker b(m_seekSlider);
        m_seekSlider->setRange(0, 0);
        m_seekSlider->setValue(0);
        m_seekSlider->setEnabled(false);
    }
    updateTimeLabel();
    m_processor->start();
    updatePlaybackControls();
}

void MainWindow::onRestart()
{
    if (m_lastSource.isEmpty()) return;
    startFile(m_lastSource, 0.0);
}

void MainWindow::onStop()
{
    if (m_isFileSource && m_processor->isRunning()) {
        // Snapshot position so Play can resume from here.
        m_lastPosSec = m_processor->currentPositionSec();
    }
    m_processor->requestStop();
}

void MainWindow::onPlayPause()
{
    // Resume from saved position if nothing is running.
    if (!m_processor->isRunning()) {
        if (!m_lastSource.isEmpty()) {
            startFile(m_lastSource, m_lastPosSec);
        }
        return;
    }
    if (!m_isFileSource) return;
    const bool nowPaused = !m_processor->isPaused();
    m_processor->setPaused(nowPaused);
    //m_btnPlayPause->setText(nowPaused ? tr("Play") : tr("Pause"));
    if (nowPaused) {
        m_btnPlayPause->setIcon(IconManager::instance().getIcon("play"));
    } else {
        m_btnPlayPause->setIcon(IconManager::instance().getIcon("pause"));
    }
}

void MainWindow::onSeekForward()
{
    if (!m_processor->isRunning() || !m_isFileSource) return;
    m_processor->seekRelativeSeconds(kSeekStepSeconds);
}

void MainWindow::onSeekBackward()
{
    if (!m_processor->isRunning() || !m_isFileSource) return;
    m_processor->seekRelativeSeconds(-kSeekStepSeconds);
}

void MainWindow::onSpeedUp()
{
    double s = m_processor->speed() * kSpeedStep;
    if (s > kSpeedMax) s = kSpeedMax;
    m_processor->setSpeed(s);
    updateSpeedLabel();
}

void MainWindow::onSpeedDown()
{
    double s = m_processor->speed() / kSpeedStep;
    if (s < kSpeedMin) s = kSpeedMin;
    m_processor->setSpeed(s);
    updateSpeedLabel();
}

void MainWindow::onResetSpeed()
{
    m_processor->setSpeed(1.0);
    updateSpeedLabel();
}

void MainWindow::onZoomIn()
{
    const QSize vp = m_scrollArea->viewport()->size();
    zoomBy(kZoomStep, QPointF(vp.width() / 2.0, vp.height() / 2.0));
}

void MainWindow::onZoomOut()
{
    const QSize vp = m_scrollArea->viewport()->size();
    zoomBy(1.0 / kZoomStep, QPointF(vp.width() / 2.0, vp.height() / 2.0));
}

void MainWindow::onResetZoom()
{
    m_zoom = 1.0;
    renderFrame();
    m_scrollArea->horizontalScrollBar()->setValue(0);
    m_scrollArea->verticalScrollBar()->setValue(0);
}

void MainWindow::zoomBy(double factor, QPointF viewportFocus)
{
    double z = m_zoom * factor;
    if (z < kZoomMin) z = kZoomMin;
    if (z > kZoomMax) z = kZoomMax;
    if (qFuzzyCompare(z, m_zoom)) return;

    // Without a frame, just apply the zoom (will render once a frame arrives).
    if (m_currentFrame.isNull()) {
        m_zoom = z;
        renderFrame();
        return;
    }

    // Snapshot the image-space point currently under the cursor.
    const QPixmap pmCur = m_videoLabel->pixmap(Qt::ReturnByValue);
    if (pmCur.isNull() || pmCur.width() <= 0 || pmCur.height() <= 0) {
        m_zoom = z;
        renderFrame();
        return;
    }
    const QPoint widgetTL = m_videoLabel->mapTo(m_scrollArea->viewport(), QPoint(0, 0));
    const QPointF inLabel = viewportFocus - QPointF(widgetTL);
    const double imgX = inLabel.x() * double(m_currentFrame.width())  / pmCur.width();
    const double imgY = inLabel.y() * double(m_currentFrame.height()) / pmCur.height();

    m_zoom = z;
    renderFrame();

    // Translate that same image point back into new label coordinates and
    // adjust scrollbars so it lands at viewportFocus again.
    const QPixmap pmNew = m_videoLabel->pixmap(Qt::ReturnByValue);
    if (pmNew.isNull() || pmNew.width() <= 0 || pmNew.height() <= 0) return;
    const double newInLabelX = imgX * double(pmNew.width())  / m_currentFrame.width();
    const double newInLabelY = imgY * double(pmNew.height()) / m_currentFrame.height();

    auto *hbar = m_scrollArea->horizontalScrollBar();
    auto *vbar = m_scrollArea->verticalScrollBar();
    const QPoint newWidgetTL = m_videoLabel->mapTo(m_scrollArea->viewport(),
                                                   QPoint(0, 0));
    // current viewport position of the image point:
    const double curVpX = newWidgetTL.x() + newInLabelX;
    const double curVpY = newWidgetTL.y() + newInLabelY;
    // delta to apply via scrollbars:
    const int dx = static_cast<int>(curVpX - viewportFocus.x());
    const int dy = static_cast<int>(curVpY - viewportFocus.y());
    hbar->setValue(hbar->value() + dx);
    vbar->setValue(vbar->value() + dy);
}

void MainWindow::onToggleRecord(bool checked)
{
    // Keep the menu and button in sync.
    if (m_actRecord && m_actRecord->isChecked() != checked) {
        QSignalBlocker b(m_actRecord);
        m_actRecord->setChecked(checked);
    }
    if (m_btnRecord->isChecked() != checked) {
        QSignalBlocker b(m_btnRecord);
        m_btnRecord->setChecked(checked);
    }

    if (checked) {
        if (!m_processor->isRunning()) {
            QMessageBox::information(this, tr("Record"),
                tr("Start a video or camera first, then enable recording."));
            QSignalBlocker bb(m_btnRecord);
            QSignalBlocker ba(m_actRecord);
            m_btnRecord->setChecked(false);
            m_actRecord->setChecked(false);
            return;
        }
        const QString outPath = nextRecordingPath();
        if (outPath.isEmpty()) {
            QMessageBox::warning(this, tr("Record"),
                tr("Could not create recording directory."));
            QSignalBlocker bb(m_btnRecord);
            QSignalBlocker ba(m_actRecord);
            m_btnRecord->setChecked(false);
            m_actRecord->setChecked(false);
            return;
        }
        m_processor->requestStartRecording(outPath);
    } else {
        m_processor->requestStopRecording();
    }
}

QString MainWindow::nextRecordingPath() const
{
    const QString ext = m_formatCombo
        ? m_formatCombo->currentText().toLower()
        : QStringLiteral("avi");
    const QString stamp = QDateTime::currentDateTime()
                              .toString(QStringLiteral("yyyyMMddHHmmss"));
    QDir base(QCoreApplication::applicationDirPath());
    if (!base.mkpath(QStringLiteral("LOCAL_DATA/tmp"))) {
        return QString();
    }
    return base.filePath(QStringLiteral("LOCAL_DATA/tmp/record_%1.%2")
                             .arg(stamp, ext));
}

void MainWindow::onRecordingChanged(bool recording)
{
    QSignalBlocker bb(m_btnRecord);
    QSignalBlocker ba(m_actRecord);
    m_btnRecord->setChecked(recording);
    m_actRecord->setChecked(recording);
    m_btnRecord->setIcon(IconManager::instance().getIcon(
        recording ? "stop" : "record"));
    m_btnRecord->setToolTip(recording
        ? tr("Haz clic para detener la grabación")
        : tr("Haz clic para grabar fragmento"));
    if (recording) {
        m_btnRecord->setStyleSheet(
            QStringLiteral("QPushButton { background-color: #d62828; }"
                           "QPushButton:hover { background-color: #e63946; }"));
    } else {
        m_btnRecord->setStyleSheet(QString());
    }
}

void MainWindow::onDurationChanged(double seconds)
{
    m_durationSec = seconds;
    QSignalBlocker b(m_seekSlider);
    if (seconds > 0.0) {
        int maxMs = static_cast<int>(seconds * 1000.0);
        if (maxMs > kSliderMaxMs) maxMs = kSliderMaxMs;
        m_seekSlider->setRange(0, maxMs);
        m_seekSlider->setEnabled(true);
    } else {
        m_seekSlider->setRange(0, 0);
        m_seekSlider->setEnabled(false);
    }
    updateTimeLabel();
}

void MainWindow::onPositionChanged(double seconds)
{
    m_positionSec = seconds;
    if (!m_userScrubbing) {
        QSignalBlocker b(m_seekSlider);
        m_seekSlider->setValue(static_cast<int>(seconds * 1000.0));
    }
    updateTimeLabel();
}

void MainWindow::onSliderMoved(int value)
{
    // Live-scrub: seek immediately so the user previews the target frame.
    m_userScrubbing = true;
    if (m_isFileSource) {
        m_processor->seekToSeconds(value / 1000.0);
    }
    m_positionSec = value / 1000.0;
    updateTimeLabel();
}

void MainWindow::onSliderReleased()
{
    if (m_isFileSource) {
        m_processor->seekToSeconds(m_seekSlider->value() / 1000.0);
    }
    m_userScrubbing = false;
}

void MainWindow::onProcessorFinished()
{
    // Capture last known position so Play can resume.
    if (m_isFileSource) {
        const double pos = m_processor->currentPositionSec();
        if (pos > 0.0) m_lastPosSec = pos;
    }
    m_isFileSource = false;
    //m_btnPlayPause->setText(tr("Play"));
    //m_btnPlayPause->setIcon(QIcon::fromTheme("media-playback-start"));
    m_btnPlayPause->setIcon(IconManager::instance().getIcon("play"));
    updatePlaybackControls();
}

void MainWindow::onError(const QString &message)
{
    QMessageBox::warning(this, tr("Video error"), message);
}

void MainWindow::updatePlaybackControls()
{
    const bool running    = m_processor->isRunning();
    const bool fileActive = m_isFileSource && running;
    const bool canResume  = !running && !m_lastSource.isEmpty();
    m_btnPlayPause->setEnabled(fileActive || canResume);
    m_btnForward->setEnabled(fileActive);
    m_btnBackward->setEnabled(fileActive);
    m_btnRestart->setEnabled(!m_lastSource.isEmpty());
    m_btnRecord->setEnabled(running);
    if (m_actRecord) m_actRecord->setEnabled(running);
    if (m_seekSlider) m_seekSlider->setEnabled(fileActive && m_durationSec > 0.0);
    //if (canResume) m_btnPlayPause->setText(tr("Play"));
    //if (canResume) m_btnPlayPause->setIcon(QIcon::fromTheme("media-playback-start"));
    if (canResume) m_btnPlayPause->setIcon(IconManager::instance().getIcon("play"));
}

void MainWindow::updateSpeedLabel()
{
    m_speedLabel->setText(tr("Speed: %1x").arg(m_processor->speed(), 0, 'f', 2));
}

QString MainWindow::formatTime(double seconds)
{
    if (seconds < 0.0 || !std::isfinite(seconds)) seconds = 0.0;
    const int totalMs = static_cast<int>(seconds * 1000.0);
    if (seconds >= 3600.0) {
        return QTime(0, 0).addMSecs(totalMs).toString(QStringLiteral("HH:mm:ss"));
    }
    return QTime(0, 0).addMSecs(totalMs).toString(QStringLiteral("mm:ss"));
}

void MainWindow::updateTimeLabel()
{
    m_timeLabel->setText(QStringLiteral("%1 / %2")
                             .arg(formatTime(m_positionSec),
                                  formatTime(m_durationSec)));
}

void MainWindow::onOpenRecentTriggered()
{
    auto *act = qobject_cast<QAction *>(sender());
    if (!act) return;
    const QString path = act->data().toString();
    if (path.isEmpty()) return;
    if (!QFileInfo::exists(path)) {
        QMessageBox::warning(this, tr("File not found"),
                             tr("The file no longer exists:\n%1").arg(path));
        m_recentVideos.removeAll(path);
        saveRecent();
        rebuildRecentMenu();
        return;
    }
    startFile(path, 0.0);
}

void MainWindow::onClearRecent()
{
    m_recentVideos.clear();
    saveRecent();
    rebuildRecentMenu();
}

void MainWindow::addToRecent(const QString &path)
{
    if (path.isEmpty()) return;
    m_recentVideos.removeAll(path);
    m_recentVideos.prepend(path);
    while (m_recentVideos.size() > kMaxRecent) {
        m_recentVideos.removeLast();
    }
    saveRecent();
    rebuildRecentMenu();
}

void MainWindow::loadRecent()
{
    QSettings settings;
    m_recentVideos = settings.value(kRecentKey).toStringList();
}

void MainWindow::saveRecent()
{
    QSettings settings;
    settings.setValue(kRecentKey, m_recentVideos);
}

void MainWindow::rebuildRecentMenu()
{
    m_recentMenu->clear();
    if (m_recentVideos.isEmpty()) {
        QAction *empty = m_recentMenu->addAction(tr("(empty)"));
        empty->setEnabled(false);
        return;
    }
    int idx = 1;
    for (const QString &path : m_recentVideos) {
        const QString label = QStringLiteral("&%1  %2")
                                  .arg(idx++)
                                  .arg(QFileInfo(path).fileName());
        QAction *act = m_recentMenu->addAction(label);
        act->setData(path);
        act->setToolTip(path);
        connect(act, &QAction::triggered, this, &MainWindow::onOpenRecentTriggered);
    }
    m_recentMenu->addSeparator();
    m_clearRecentAction = m_recentMenu->addAction(tr("&Clear list"));
    connect(m_clearRecentAction, &QAction::triggered,
            this, &MainWindow::onClearRecent);
}

// ---------- Markers ----------

void MainWindow::onAddPlayerMarker(bool checked)
{
    if (checked) {
        m_pendingMarker = PendingMarker::Player;
        m_pendingMoveIndex = -1;
        if (m_btnAddDisc->isChecked()) {
            QSignalBlocker b(m_btnAddDisc);
            m_btnAddDisc->setChecked(false);
        }
        if (m_btnMoveMarker && m_btnMoveMarker->isChecked()) {
            QSignalBlocker b(m_btnMoveMarker);
            m_btnMoveMarker->setChecked(false);
        }
        m_videoLabel->setCursor(Qt::CrossCursor);
    } else if (m_pendingMarker == PendingMarker::Player) {
        m_pendingMarker = PendingMarker::None;
        m_videoLabel->unsetCursor();
    }
}

void MainWindow::onAddDiscMarker(bool checked)
{
    if (checked) {
        m_pendingMarker = PendingMarker::Disc;
        m_pendingMoveIndex = -1;
        if (m_btnAddPlayer->isChecked()) {
            QSignalBlocker b(m_btnAddPlayer);
            m_btnAddPlayer->setChecked(false);
        }
        if (m_btnMoveMarker && m_btnMoveMarker->isChecked()) {
            QSignalBlocker b(m_btnMoveMarker);
            m_btnMoveMarker->setChecked(false);
        }
        m_videoLabel->setCursor(Qt::CrossCursor);
    } else if (m_pendingMarker == PendingMarker::Disc) {
        m_pendingMarker = PendingMarker::None;
        m_videoLabel->unsetCursor();
    }
}

void MainWindow::cancelMarkerPlacement()
{
    m_pendingMarker = PendingMarker::None;
    m_pendingMoveIndex = -1;
    if (m_btnAddPlayer->isChecked()) {
        QSignalBlocker b(m_btnAddPlayer);
        m_btnAddPlayer->setChecked(false);
    }
    if (m_btnAddDisc->isChecked()) {
        QSignalBlocker b(m_btnAddDisc);
        m_btnAddDisc->setChecked(false);
    }
    if (m_btnMoveMarker && m_btnMoveMarker->isChecked()) {
        QSignalBlocker b(m_btnMoveMarker);
        m_btnMoveMarker->setChecked(false);
    }
    m_videoLabel->unsetCursor();
}

void MainWindow::placeMarkerAt(int imgX, int imgY)
{
    if (m_pendingMarker == PendingMarker::None) return;

    if (m_pendingMarker == PendingMarker::MoveExisting) {
        if (m_pendingMoveIndex >= 0 && m_pendingMoveIndex < m_markers.size()) {
            m_markers[m_pendingMoveIndex].x = imgX;
            m_markers[m_pendingMoveIndex].y = imgY;
            m_markersDirty = true;
        }
        cancelMarkerPlacement();
        refreshMarkerList();
        renderFrame();
        return;
    }

    Marker m;
    m.type    = (m_pendingMarker == PendingMarker::Player)
                    ? QStringLiteral("player")
                    : QStringLiteral("disc");
    m.x       = imgX;
    m.y       = imgY;
    m.timeSec = m_positionSec;
    m_markers.append(m);
    m_markersDirty = true;
    cancelMarkerPlacement();
    refreshMarkerList();
    renderFrame();
}

void MainWindow::onClearMarkers()
{
    if (m_markers.isEmpty()) return;
    m_markers.clear();
    m_markersDirty = false;   // nothing left to save
    refreshMarkerList();
    renderFrame();
}

void MainWindow::onToggleMarkersVisible(bool checked)
{
    m_markersVisible = checked;
    renderFrame();
}

QString MainWindow::markersBaseDirForCurrentVideo() const
{
    if (m_lastSource.isEmpty()) return QString();
    const QString base = QFileInfo(m_lastSource).completeBaseName();
    return QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("LOCAL_DATA/configs/%1").arg(base));
}

QString MainWindow::nextMarkersPath() const
{
    if (m_lastSource.isEmpty()) return QString();
    const QString base = QFileInfo(m_lastSource).completeBaseName();
    QDir baseDir(QCoreApplication::applicationDirPath());
    const QString sub = QStringLiteral("LOCAL_DATA/configs/%1").arg(base);
    if (!baseDir.mkpath(sub)) return QString();
    const QString stamp = QDateTime::currentDateTime()
                              .toString(QStringLiteral("yyyyMMddHHmmss"));
    return baseDir.filePath(QStringLiteral("%1/markers_%2.yml").arg(sub, stamp));
}

bool MainWindow::saveMarkersToFile(const QString &path)
{
    cv::FileStorage fs(path.toStdString(), cv::FileStorage::WRITE);
    if (!fs.isOpened()) return false;
    const ShotMetadata meta = currentMetadataFromForm();
    fs << "video"        << m_lastSource.toStdString();
    fs << "player"       << meta.playerName.toStdString();
    fs << "pdga"         << meta.pdgaNumber.toStdString();
    fs << "course"       << meta.courseName.toStdString();
    fs << "courseLayout" << meta.courseLayout.toStdString();
    fs << "hole"         << meta.hole.toStdString();
    fs << "count"        << m_markers.size();
    fs << "markers" << "[";
    for (const Marker &m : m_markers) {
        fs << "{"
           << "type" << m.type.toStdString()
           << "x"    << m.x
           << "y"    << m.y
           << "time" << m.timeSec
           << "}";
    }
    fs << "]";
    fs.release();
    m_markersDirty = false;
    return true;
}

bool MainWindow::loadMarkersFromFile(const QString &path)
{
    cv::FileStorage fs(path.toStdString(), cv::FileStorage::READ);
    if (!fs.isOpened()) return false;

    QVector<Marker> loaded;
    cv::FileNode node = fs["markers"];
    if (node.isSeq()) {
        for (auto it = node.begin(); it != node.end(); ++it) {
            Marker m;
            std::string type;
            (*it)["type"] >> type;
            m.type = QString::fromStdString(type);
            (*it)["x"]    >> m.x;
            (*it)["y"]    >> m.y;
            (*it)["time"] >> m.timeSec;
            if (m.type != QLatin1String("player") && m.type != QLatin1String("disc")) {
                continue;
            }
            loaded.append(m);
        }
    }

    auto readStr = [&fs](const char *key, QString &out) {
        cv::FileNode n = fs[key];
        if (n.empty() || !n.isString()) return;
        std::string s; n >> s;
        out = QString::fromStdString(s);
    };
    ShotMetadata meta;
    readStr("player",       meta.playerName);
    readStr("pdga",         meta.pdgaNumber);
    readStr("course",       meta.courseName);
    readStr("courseLayout", meta.courseLayout);
    readStr("hole",         meta.hole);
    applyMetadataToForm(meta);

    fs.release();
    m_markers = loaded;
    m_markersDirty = false;
    refreshMarkerList();
    return true;
}

void MainWindow::onSaveMarkers()
{
    if (m_lastSource.isEmpty()) {
        QMessageBox::information(this, tr("Save Markers"),
            tr("Open a video first — markers are stored per video."));
        return;
    }
    if (m_markers.isEmpty()) {
        QMessageBox::information(this, tr("Save Markers"),
            tr("There are no markers to save."));
        return;
    }
    const QString path = nextMarkersPath();
    if (path.isEmpty() || !saveMarkersToFile(path)) {
        QMessageBox::warning(this, tr("Save Markers"),
            tr("Failed to write markers file."));
        return;
    }
    statusBar()->showMessage(tr("Saved %1 markers to %2")
                                 .arg(m_markers.size())
                                 .arg(path),
                             5000);
    refreshMarkerFileList();
}

void MainWindow::onLoadMarkers()
{
    QString startDir = markersBaseDirForCurrentVideo();
    if (startDir.isEmpty() || !QDir(startDir).exists()) {
        startDir = QDir(QCoreApplication::applicationDirPath())
                       .filePath(QStringLiteral("LOCAL_DATA/configs"));
        if (!QDir(startDir).exists()) startDir = QDir::homePath();
    }
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Load markers"), startDir, tr("Markers (*.yml *.yaml)"));
    if (path.isEmpty()) return;
    if (!loadMarkersFromFile(path)) {
        QMessageBox::warning(this, tr("Load Markers"),
            tr("Failed to read markers file."));
        return;
    }
    statusBar()->showMessage(tr("Loaded %1 markers from %2")
                                 .arg(m_markers.size())
                                 .arg(path),
                             5000);
    renderFrame();
}

// ---------- Sidebars ----------

QWidget *MainWindow::buildLeftSidebar()
{
    auto *root = new QWidget;
    auto *vbox = new QVBoxLayout(root);
    vbox->setContentsMargins(6, 6, 6, 6);

    auto *grpMarkers = new QGroupBox(tr("Markers"));
    auto *gv = new QVBoxLayout(grpMarkers);
    auto *addRow = new QHBoxLayout;
    addRow->addWidget(m_btnAddPlayer);
    addRow->addWidget(m_btnAddDisc);
    addRow->addWidget(m_btnSaveMarkers);
    addRow->addStretch();
    gv->addLayout(addRow);
    vbox->addWidget(grpMarkers);

    auto *grpCurrent = new QGroupBox(tr("Current Markers"));
    auto *cv = new QVBoxLayout(grpCurrent);
    m_markerList = new QListWidget;
    m_markerList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_markerList->setToolTip(tr("Double-click a marker to seek there"));
    cv->addWidget(m_markerList);
    auto *editRow = new QHBoxLayout;
    m_btnDeleteMarker = new QPushButton(tr("Delete"));
    m_btnToggleType   = new QPushButton(tr("Toggle Type"));
    m_btnMoveMarker   = new QPushButton(tr("Move"));
    m_btnMoveMarker->setCheckable(true);
    m_btnDeleteMarker->setToolTip(tr("Remove the selected marker"));
    m_btnToggleType->setToolTip(tr("Switch player ↔ disc for the selected marker"));
    m_btnMoveMarker->setToolTip(tr("Click, then click on the video to reposition the selected marker"));
    editRow->addWidget(m_btnDeleteMarker);
    editRow->addWidget(m_btnToggleType);
    editRow->addWidget(m_btnMoveMarker);
    cv->addLayout(editRow);
    connect(m_markerList, &QListWidget::itemClicked,
            this, &MainWindow::onMarkerListActivated);
    connect(m_markerList, &QListWidget::itemActivated,
            this, &MainWindow::onMarkerListActivated);
    connect(m_markerList, &QListWidget::itemDoubleClicked,
            this, &MainWindow::onMarkerListActivated);
    // Cover keyboard navigation (arrow keys) too — fires when the row changes.
    connect(m_markerList, &QListWidget::currentItemChanged,
            this, [this](QListWidgetItem *current, QListWidgetItem *) {
                onMarkerListActivated(current);
            });
    connect(m_btnDeleteMarker, &QPushButton::clicked,
            this, &MainWindow::onDeleteSelectedMarker);
    connect(m_btnToggleType, &QPushButton::clicked,
            this, &MainWindow::onToggleSelectedMarkerType);
    connect(m_btnMoveMarker, &QPushButton::toggled,
            this, &MainWindow::onMoveSelectedMarker);
    vbox->addWidget(grpCurrent, 1);

    auto *grpMeta = new QGroupBox(tr("Shot Metadata"));
    auto *form = new QFormLayout(grpMeta);
    m_edPlayerName   = new QLineEdit;
    m_edPdgaNumber   = new QLineEdit;
    m_edPdgaNumber->setPlaceholderText(tr("e.g. 12345"));
    m_edCourseName   = new QLineEdit;
    m_edCourseLayout = new QLineEdit;
    m_spHole         = new QSpinBox;
    m_spHole->setRange(0, 99);
    m_spHole->setSpecialValueText(tr("—"));
    form->addRow(tr("Player:"),       m_edPlayerName);
    form->addRow(tr("PDGA #:"),       m_edPdgaNumber);
    form->addRow(tr("Course:"),       m_edCourseName);
    form->addRow(tr("Course layout:"),m_edCourseLayout);
    form->addRow(tr("Hole:"),         m_spHole);
    vbox->addWidget(grpMeta);

    auto *grpFiles = new QGroupBox(tr("Marker Files"));
    auto *fv = new QVBoxLayout(grpFiles);
    m_markerFileList = new QListWidget;
    m_markerFileList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_markerFileList->setToolTip(tr("Double-click a file to load its markers"));
    fv->addWidget(m_markerFileList);
    connect(m_markerFileList, &QListWidget::itemActivated,
            this, &MainWindow::onMarkerFileActivated);
    connect(m_markerFileList, &QListWidget::itemDoubleClicked,
            this, &MainWindow::onMarkerFileActivated);
    vbox->addWidget(grpFiles);

    return root;
}

QWidget *MainWindow::buildRightSidebar()
{
    auto *root = new QWidget;
    auto *vbox = new QVBoxLayout(root);
    vbox->setContentsMargins(6, 6, 6, 6);

    auto *grpFlight = new QGroupBox(tr("Flight Telemetry"));
    auto *form = new QFormLayout(grpFlight);
    m_lblSpin     = new QLabel(QStringLiteral("—"));
    m_lblNose     = new QLabel(QStringLiteral("—"));
    m_lblApex     = new QLabel(QStringLiteral("—"));
    m_lblDistance = new QLabel(QStringLiteral("—"));
    form->addRow(tr("Spin:"),     m_lblSpin);
    form->addRow(tr("Nose:"),     m_lblNose);
    form->addRow(tr("Apex:"),     m_lblApex);
    form->addRow(tr("Distance:"), m_lblDistance);
    vbox->addWidget(grpFlight);

    auto *grpPose = new QGroupBox(tr("Pose Joint Angles"));
    auto *pv = new QVBoxLayout(grpPose);
    m_poseJointList = new QListWidget;
    m_poseJointList->setSelectionMode(QAbstractItemView::NoSelection);
    pv->addWidget(m_poseJointList);
    vbox->addWidget(grpPose, 1);

    // ---- Result Overlays ----
    auto *grpOverlay = new QGroupBox(tr("Result Overlays"));
    auto *ov = new QVBoxLayout(grpOverlay);

    m_btnShowDiscTrack = new QPushButton(tr("Show disc trajectory"));
    m_btnShowDiscTrack->setCheckable(true);
    m_btnShowDiscTrack->setToolTip(
        tr("Overlay the disc_tracker trajectory on playback.\n"
           "Auto-loads the most recent CSV for the current video."));
    m_btnShowSkeleton  = new QPushButton(tr("Show player skeleton"));
    m_btnShowSkeleton->setCheckable(true);
    m_btnShowSkeleton->setToolTip(
        tr("Overlay the player_tracker skeleton on playback.\n"
           "Auto-loads the most recent CSV for the current video."));
    ov->addWidget(m_btnShowDiscTrack);
    ov->addWidget(m_btnShowSkeleton);

    auto *srcRow = new QHBoxLayout;
    srcRow->setContentsMargins(0, 0, 0, 0);
    m_btnSrcOriginal  = new QPushButton(tr("Original"));
    m_btnSrcHighlight = new QPushButton(tr("Highlight"));
    m_btnSrcZoom      = new QPushButton(tr("Zoom"));
    m_btnSrcOriginal->setToolTip(tr("Switch playback back to the original video."));
    m_btnSrcHighlight->setToolTip(
        tr("Play the most recent player_highlighter overlay video for this video."));
    m_btnSrcZoom->setToolTip(
        tr("Play the most recent player_highlighter zoom track for this video."));
    srcRow->addWidget(m_btnSrcOriginal);
    srcRow->addWidget(m_btnSrcHighlight);
    srcRow->addWidget(m_btnSrcZoom);
    ov->addLayout(srcRow);

    connect(m_btnShowDiscTrack, &QPushButton::toggled,
            this, &MainWindow::onToggleDiscOverlay);
    connect(m_btnShowSkeleton, &QPushButton::toggled,
            this, &MainWindow::onToggleSkeletonOverlay);
    connect(m_btnSrcOriginal,  &QPushButton::clicked,
            this, &MainWindow::onSwitchSourceOriginal);
    connect(m_btnSrcHighlight, &QPushButton::clicked,
            this, &MainWindow::onSwitchSourceHighlight);
    connect(m_btnSrcZoom,      &QPushButton::clicked,
            this, &MainWindow::onSwitchSourceZoom);

    vbox->addWidget(grpOverlay);

    buildSubservicesGroup(vbox);

    return root;
}

void MainWindow::buildSubservicesGroup(QVBoxLayout *parent)
{
    auto *grp = new QGroupBox(tr("Subservices"));
    auto *gv  = new QVBoxLayout(grp);

    auto makeRow = [](QPushButton *gen, QPushButton *run) {
        auto *h = new QHBoxLayout;
        h->setContentsMargins(0, 0, 0, 0);
        h->addWidget(gen, 1);
        h->addWidget(run, 1);
        return h;
    };

    m_btnGenDiscCfg        = new QPushButton(tr("Gen disc cfg"));
    m_btnRunDisc           = new QPushButton(tr("Run disc tracker"));
    m_btnGenPlayerCfg      = new QPushButton(tr("Gen player cfg"));
    m_btnRunPlayer         = new QPushButton(tr("Run player tracker"));
    m_btnGenHighlighterCfg = new QPushButton(tr("Gen highlighter cfg"));
    m_btnRunHighlighter    = new QPushButton(tr("Run highlighter"));
    m_btnLoadStats         = new QPushButton(tr("Load stats from CSV..."));

    m_btnGenDiscCfg->setToolTip(
        tr("Auto-save markers if dirty, then write a YAML config\n"
           "for disc_tracker into LOCAL_DATA/configs/<video>/."));
    m_btnGenPlayerCfg->setToolTip(
        tr("Auto-save markers, then write a YAML config for player_tracker."));
    m_btnGenHighlighterCfg->setToolTip(
        tr("Write a YAML config for player_highlighter pointing at the\n"
           "most recent player_tracker CSV for this video."));
    m_btnRunDisc->setToolTip(
        tr("Spawn disc_tracker with the most recent generated config."));
    m_btnRunPlayer->setToolTip(
        tr("Spawn player_tracker with the most recent generated config."));
    m_btnRunHighlighter->setToolTip(
        tr("Spawn player_highlighter with the most recent generated config."));
    m_btnLoadStats->setToolTip(
        tr("Pick a player_tracker CSV and compute joint angles\n"
           "(elbows, shoulders, knees) into the Pose panel above."));

    // Run buttons start disabled — enabled once a config has been generated.
    m_btnRunDisc->setEnabled(false);
    m_btnRunPlayer->setEnabled(false);
    m_btnRunHighlighter->setEnabled(false);

    gv->addLayout(makeRow(m_btnGenDiscCfg,        m_btnRunDisc));
    gv->addLayout(makeRow(m_btnGenPlayerCfg,      m_btnRunPlayer));
    gv->addLayout(makeRow(m_btnGenHighlighterCfg, m_btnRunHighlighter));
    gv->addWidget(m_btnLoadStats);

    m_subserviceLog = new QPlainTextEdit;
    m_subserviceLog->setReadOnly(true);
    m_subserviceLog->setPlaceholderText(
        tr("Subservice output and config-generation messages appear here."));
    m_subserviceLog->setMaximumBlockCount(2000);
    QFont mono = m_subserviceLog->font();
    mono.setStyleHint(QFont::Monospace);
    mono.setFamily(QStringLiteral("Consolas"));
    m_subserviceLog->setFont(mono);
    m_subserviceLog->setMinimumHeight(140);
    gv->addWidget(m_subserviceLog, 1);

    connect(m_btnGenDiscCfg,        &QPushButton::clicked,
            this, &MainWindow::onGenerateDiscConfig);
    connect(m_btnGenPlayerCfg,      &QPushButton::clicked,
            this, &MainWindow::onGeneratePlayerConfig);
    connect(m_btnGenHighlighterCfg, &QPushButton::clicked,
            this, &MainWindow::onGenerateHighlighterConfig);
    connect(m_btnRunDisc,           &QPushButton::clicked,
            this, &MainWindow::onRunDiscTracker);
    connect(m_btnRunPlayer,         &QPushButton::clicked,
            this, &MainWindow::onRunPlayerTracker);
    connect(m_btnRunHighlighter,    &QPushButton::clicked,
            this, &MainWindow::onRunPlayerHighlighter);
    connect(m_btnLoadStats,         &QPushButton::clicked,
            this, &MainWindow::onLoadStats);

    parent->addWidget(grp, 1);
}

// ---------- Marker file list ----------

void MainWindow::refreshMarkerFileList()
{
    if (!m_markerFileList) return;
    m_markerFileList->clear();
    const QString dir = markersBaseDirForCurrentVideo();
    if (dir.isEmpty() || !QDir(dir).exists()) return;

    QDir d(dir);
    const QStringList filters{QStringLiteral("markers_*.yml"),
                              QStringLiteral("markers_*.yaml")};
    QFileInfoList files = d.entryInfoList(filters,
                                          QDir::Files | QDir::NoDotAndDotDot,
                                          QDir::Time);
    static const QRegularExpression stampRe(
        QStringLiteral("^markers_(\\d{8})(\\d{6})$"));
    for (const QFileInfo &fi : files) {
        QString display;
        const auto m = stampRe.match(fi.completeBaseName());
        if (m.hasMatch()) {
            const QString date = m.captured(1);  // YYYYMMDD
            const QString tm   = m.captured(2);  // HHMMSS
            display = QStringLiteral("%1 - %2:%3:%4")
                          .arg(date, tm.left(2), tm.mid(2, 2), tm.right(2));
        } else {
            display = fi.fileName();  // unrecognized format — show raw name
        }

        // Pull the player name out of the YAML so it can be appended.
        QString playerName;
        cv::FileStorage fs(fi.absoluteFilePath().toStdString(),
                           cv::FileStorage::READ);
        if (fs.isOpened()) {
            cv::FileNode n = fs["player"];
            if (!n.empty() && n.isString()) {
                std::string s; n >> s;
                playerName = QString::fromStdString(s).trimmed();
            }
            fs.release();
        }
        if (!playerName.isEmpty()) {
            display = QStringLiteral("%1 - %2").arg(display, playerName);
        }

        auto *item = new QListWidgetItem(display);
        item->setData(Qt::UserRole, fi.absoluteFilePath());
        item->setToolTip(fi.absoluteFilePath());
        m_markerFileList->addItem(item);
    }
}

void MainWindow::onMarkerFileActivated(QListWidgetItem *item)
{
    if (!item) return;
    const QString path = item->data(Qt::UserRole).toString();
    if (path.isEmpty()) return;
    if (!loadMarkersFromFile(path)) {
        QMessageBox::warning(this, tr("Load Markers"),
            tr("Failed to read markers file."));
        return;
    }
    statusBar()->showMessage(tr("Loaded %1 markers from %2")
                                 .arg(m_markers.size())
                                 .arg(path),
                             5000);
    renderFrame();
}

// ---------- Metadata form ----------

ShotMetadata MainWindow::currentMetadataFromForm() const
{
    ShotMetadata meta;
    if (m_edPlayerName)   meta.playerName   = m_edPlayerName->text().trimmed();
    if (m_edPdgaNumber)   meta.pdgaNumber   = m_edPdgaNumber->text().trimmed();
    if (m_edCourseName)   meta.courseName   = m_edCourseName->text().trimmed();
    if (m_edCourseLayout) meta.courseLayout = m_edCourseLayout->text().trimmed();
    if (m_spHole && m_spHole->value() > 0) {
        meta.hole = QString::number(m_spHole->value());
    }
    return meta;
}

void MainWindow::applyMetadataToForm(const ShotMetadata &meta)
{
    if (m_edPlayerName)   m_edPlayerName->setText(meta.playerName);
    if (m_edPdgaNumber)   m_edPdgaNumber->setText(meta.pdgaNumber);
    if (m_edCourseName)   m_edCourseName->setText(meta.courseName);
    if (m_edCourseLayout) m_edCourseLayout->setText(meta.courseLayout);
    if (m_spHole) {
        bool ok = false;
        const int v = meta.hole.toInt(&ok);
        m_spHole->setValue(ok ? v : 0);
    }
}

void MainWindow::clearMetadataForm()
{
    applyMetadataToForm(ShotMetadata{});
}

// ---------- Telemetry ----------

void MainWindow::clearTelemetry()
{
    if (m_lblSpin)     m_lblSpin->setText(QStringLiteral("—"));
    if (m_lblNose)     m_lblNose->setText(QStringLiteral("—"));
    if (m_lblApex)     m_lblApex->setText(QStringLiteral("—"));
    if (m_lblDistance) m_lblDistance->setText(QStringLiteral("—"));
    if (m_poseJointList) m_poseJointList->clear();
}

void MainWindow::setSpinRpm(double rpm)
{
    if (m_lblSpin) m_lblSpin->setText(tr("%1 rpm").arg(rpm, 0, 'f', 0));
}

void MainWindow::setNoseDeg(double degrees)
{
    if (m_lblNose) m_lblNose->setText(tr("%1°").arg(degrees, 0, 'f', 1));
}

void MainWindow::setApexMeters(double meters)
{
    if (m_lblApex) m_lblApex->setText(tr("%1 m").arg(meters, 0, 'f', 2));
}

void MainWindow::setDistanceMeters(double meters)
{
    if (m_lblDistance) m_lblDistance->setText(tr("%1 m").arg(meters, 0, 'f', 1));
}

void MainWindow::setPoseJointAngles(const QVector<QPair<QString, double>> &angles)
{
    if (!m_poseJointList) return;
    m_poseJointList->clear();
    for (const auto &p : angles) {
        m_poseJointList->addItem(QStringLiteral("%1: %2°")
                                     .arg(p.first)
                                     .arg(p.second, 0, 'f', 1));
    }
}

// ---------- Current marker list (edit/delete) ----------

void MainWindow::refreshMarkerList()
{
    if (!m_markerList) return;
    // Block signals so the rebuild doesn't trigger spurious seeks via the
    // currentItemChanged / itemClicked connections.
    QSignalBlocker block(m_markerList);
    const int prev = m_markerList->currentRow();
    m_markerList->clear();
    for (int i = 0; i < m_markers.size(); ++i) {
        const Marker &m = m_markers[i];
        const QString letter = (m.type == QLatin1String("player"))
                                   ? QStringLiteral("P") : QStringLiteral("D");
        const QString text = QStringLiteral("[%1] %2 — (%3, %4)")
                                 .arg(letter)
                                 .arg(formatTime(m.timeSec))
                                 .arg(m.x).arg(m.y);
        auto *item = new QListWidgetItem(text);
        item->setData(Qt::UserRole, i);
        m_markerList->addItem(item);
    }
    if (prev >= 0 && prev < m_markerList->count()) {
        m_markerList->setCurrentRow(prev);
    }
    const bool hasItems = m_markerList->count() > 0;
    if (m_btnDeleteMarker) m_btnDeleteMarker->setEnabled(hasItems);
    if (m_btnToggleType)   m_btnToggleType->setEnabled(hasItems);
    if (m_btnMoveMarker)   m_btnMoveMarker->setEnabled(hasItems);
}

int MainWindow::selectedMarkerIndex() const
{
    if (!m_markerList) return -1;
    const int row = m_markerList->currentRow();
    if (row < 0 || row >= m_markers.size()) return -1;
    return row;
}

void MainWindow::onMarkerListActivated(QListWidgetItem *item)
{
    if (!item) return;
    const int idx = item->data(Qt::UserRole).toInt();
    if (idx < 0 || idx >= m_markers.size()) return;
    if (m_isFileSource && m_processor->isRunning()) {
        m_processor->seekToSeconds(m_markers[idx].timeSec);
    } else {
        m_positionSec = m_markers[idx].timeSec;
        renderFrame();
    }
}

void MainWindow::onDeleteSelectedMarker()
{
    const int idx = selectedMarkerIndex();
    if (idx < 0) return;
    m_markers.remove(idx);
    m_markersDirty = true;
    if (m_pendingMoveIndex == idx) cancelMarkerPlacement();
    else if (m_pendingMoveIndex > idx) --m_pendingMoveIndex;
    refreshMarkerList();
    renderFrame();
}

void MainWindow::onToggleSelectedMarkerType()
{
    const int idx = selectedMarkerIndex();
    if (idx < 0) return;
    Marker &m = m_markers[idx];
    m.type = (m.type == QLatin1String("player"))
                 ? QStringLiteral("disc")
                 : QStringLiteral("player");
    m_markersDirty = true;
    refreshMarkerList();
    renderFrame();
}

void MainWindow::onMoveSelectedMarker(bool checked)
{
    if (checked) {
        const int idx = selectedMarkerIndex();
        if (idx < 0) {
            QSignalBlocker b(m_btnMoveMarker);
            m_btnMoveMarker->setChecked(false);
            return;
        }
        m_pendingMarker = PendingMarker::MoveExisting;
        m_pendingMoveIndex = idx;
        if (m_btnAddPlayer->isChecked()) {
            QSignalBlocker b(m_btnAddPlayer);
            m_btnAddPlayer->setChecked(false);
        }
        if (m_btnAddDisc->isChecked()) {
            QSignalBlocker b(m_btnAddDisc);
            m_btnAddDisc->setChecked(false);
        }
        m_videoLabel->setCursor(Qt::CrossCursor);
    } else if (m_pendingMarker == PendingMarker::MoveExisting) {
        m_pendingMarker = PendingMarker::None;
        m_pendingMoveIndex = -1;
        m_videoLabel->unsetCursor();
    }
}

// ---------- Subservices: shared helpers ----------

void MainWindow::appendLog(const QString &line)
{
    if (!m_subserviceLog) return;
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    m_subserviceLog->appendPlainText(QStringLiteral("[%1] %2").arg(stamp, line));
}

QString MainWindow::ensureMarkersSaved()
{
    if (m_lastSource.isEmpty()) {
        QMessageBox::information(this, tr("Subservices"),
            tr("Open a video first."));
        return {};
    }
    if (m_markers.isEmpty()) {
        QMessageBox::information(this, tr("Subservices"),
            tr("Add at least one player marker (and one disc marker for\n"
               "player_tracker / player_highlighter) before generating a config."));
        return {};
    }
    if (!m_markersDirty) {
        const QString dir = markersBaseDirForCurrentVideo();
        if (!dir.isEmpty()) {
            QDir d(dir);
            const QStringList filt{QStringLiteral("markers_*.yml"),
                                    QStringLiteral("markers_*.yaml")};
            QFileInfoList files = d.entryInfoList(filt,
                QDir::Files | QDir::NoDotAndDotDot, QDir::Time);
            if (!files.isEmpty()) return files.first().absoluteFilePath();
        }
    }
    const QString path = nextMarkersPath();
    if (path.isEmpty() || !saveMarkersToFile(path)) {
        QMessageBox::warning(this, tr("Subservices"),
            tr("Auto-save of markers failed; cannot generate config."));
        return {};
    }
    refreshMarkerFileList();
    appendLog(tr("Auto-saved markers: %1").arg(path));
    statusBar()->showMessage(tr("Markers saved: %1").arg(path), 4000);
    return path;
}

QString MainWindow::findSubserviceExe(const QString &name) const
{
    const QString exeName = name + QStringLiteral(".exe");
    const QStringList candidates = {
        QStringLiteral("build_mingw_") + name,
        QStringLiteral("build_mingw"),
        QStringLiteral("build_mingw_player"),
        QStringLiteral("build_mingw_highlighter"),
        QStringLiteral("build/bin/Release"),
        QStringLiteral("build/bin"),
    };
    QDir root(QCoreApplication::applicationDirPath());
    for (int up = 0; up < 6; ++up) {
        for (const QString &c : candidates) {
            const QString p = root.filePath(c + QStringLiteral("/") + exeName);
            if (QFile::exists(p)) return QFileInfo(p).absoluteFilePath();
        }
        if (!root.cdUp()) break;
    }
    return QStandardPaths::findExecutable(name);
}

QString MainWindow::findLatestPlayerCsv() const
{
    if (m_lastSource.isEmpty()) return {};
    QDir root(QCoreApplication::applicationDirPath());
    for (int up = 0; up < 6; ++up) {
        const QString outDir = root.filePath(QStringLiteral("player_tracker/out"));
        QDir od(outDir);
        if (od.exists()) {
            const QString stem = QFileInfo(m_lastSource).completeBaseName();
            QStringList filt;
            filt << stem + QStringLiteral("_*.csv")
                 << QStringLiteral("hole*_player_*.csv")
                 << QStringLiteral("*_player_*.csv");
            QFileInfoList files = od.entryInfoList(filt,
                QDir::Files | QDir::NoDotAndDotDot, QDir::Time);
            if (!files.isEmpty()) return files.first().absoluteFilePath();
        }
        if (!root.cdUp()) break;
    }
    return {};
}

namespace {

QString makeOutPath(const QString &videoStem, const QString &service,
                    const QString &kind, const QString &ext)
{
    QDir root(QCoreApplication::applicationDirPath());
    for (int up = 0; up < 6; ++up) {
        if (QDir(root.filePath(service)).exists()) break;
        if (!root.cdUp()) { root = QDir(QCoreApplication::applicationDirPath()); break; }
    }
    const QString outDir = root.filePath(service + QStringLiteral("/out"));
    QDir().mkpath(outDir);
    return QDir(outDir).filePath(videoStem + QStringLiteral("_") + kind + ext);
}

}  // namespace

QString MainWindow::writeDiscTrackerConfig(const QString &markersPath)
{
    if (m_lastSource.isEmpty() || markersPath.isEmpty()) return {};
    const QString videoAbs = QFileInfo(m_lastSource).absoluteFilePath();
    const QString stem     = QFileInfo(m_lastSource).completeBaseName();
    const QString cfgPath  = QDir(markersBaseDirForCurrentVideo()).filePath(
        QStringLiteral("disc_tracker_%1.yaml")
            .arg(QDateTime::currentDateTime().toString("yyyyMMddHHmmss")));

    cv::FileStorage fs(cfgPath.toStdString(), cv::FileStorage::WRITE);
    if (!fs.isOpened()) return {};

    fs << "video_path"   << videoAbs.toStdString();
    fs << "markers_path" << markersPath.toStdString();
    fs << "backend"      << "hough";
    fs << "hough_dp"     << 1.2;
    fs << "hough_min_dist"   << 20.0;
    fs << "hough_param1"     << 100.0;
    fs << "hough_param2"     << 18.0;
    fs << "hough_min_radius" << 4;
    fs << "hough_max_radius" << 24;
    fs << "use_kalman"        << 1;
    fs << "max_missed_frames" << 0;
    fs << "search_radius_px"  << 0;
    fs << "seed_from_markers"       << 1;
    fs << "clip_to_marker_window"   << 1;
    fs << "marker_window_pad_sec"   << 0.4;
    fs << "marker_tolerance_frames" << 2;
    fs << "anchor_to_markers" << 1;
    fs << "anchor_radius_px"  << 45;
    fs << "anchor_fallback"   << 1;
    fs << "csv_path"           << makeOutPath(stem, "disc_tracker",
                                              "anchored_path",   ".csv").toStdString();
    fs << "overlay_video_path" << makeOutPath(stem, "disc_tracker",
                                              "anchored_overlay", ".mp4").toStdString();
    fs << "draw_trajectory"    << 1;
    fs.release();
    return cfgPath;
}

QString MainWindow::writePlayerTrackerConfig(const QString &markersPath)
{
    if (m_lastSource.isEmpty() || markersPath.isEmpty()) return {};
    const QString videoAbs = QFileInfo(m_lastSource).absoluteFilePath();
    const QString stem     = QFileInfo(m_lastSource).completeBaseName();
    const QString cfgPath  = QDir(markersBaseDirForCurrentVideo()).filePath(
        QStringLiteral("player_tracker_%1.yaml")
            .arg(QDateTime::currentDateTime().toString("yyyyMMddHHmmss")));

    QDir root(QCoreApplication::applicationDirPath());
    QString modelPath;
    for (int up = 0; up < 6; ++up) {
        const QString candidate = root.filePath(
            QStringLiteral("LOCAL_DATA/models/yolov8n-pose.onnx"));
        if (QFile::exists(candidate)) { modelPath = candidate; break; }
        if (!root.cdUp()) break;
    }
    if (modelPath.isEmpty()) {
        QMessageBox::warning(this, tr("Generate player_tracker config"),
            tr("Could not find LOCAL_DATA/models/yolov8n-pose.onnx.\n"
               "Export it via: yolo export model=yolov8n-pose.pt format=onnx imgsz=640"));
        return {};
    }

    cv::FileStorage fs(cfgPath.toStdString(), cv::FileStorage::WRITE);
    if (!fs.isOpened()) return {};

    fs << "video_path"             << videoAbs.toStdString();
    fs << "markers_path"           << markersPath.toStdString();
    fs << "model_path"             << modelPath.toStdString();
    fs << "input_size"             << 640;
    fs << "person_conf_threshold"  << 0.25;
    fs << "nms_threshold"          << 0.45;
    fs << "keypoint_vis_threshold" << 0.35;
    fs << "min_duration_sec"       << 4.0;
    fs << "max_duration_sec"       << 15.0;
    fs << "pre_pad_sec"            << 0.0;
    fs << "person_gate_radius_px"  << 250;
    fs << "use_iou_tracking"       << 1;
    fs << "use_keypoint_kalman"    << 1;
    fs << "csv_path"           << makeOutPath(stem, "player_tracker",
                                              "player",         ".csv").toStdString();
    fs << "overlay_video_path" << makeOutPath(stem, "player_tracker",
                                              "player_overlay", ".mp4").toStdString();
    fs << "draw_all_keypoints"    << 1;
    fs << "label_priority_joints" << 1;
    fs.release();
    return cfgPath;
}

QString MainWindow::writePlayerHighlighterConfig(const QString &playerCsvPath)
{
    if (m_lastSource.isEmpty() || playerCsvPath.isEmpty()) return {};
    const QString videoAbs = QFileInfo(m_lastSource).absoluteFilePath();
    const QString stem     = QFileInfo(m_lastSource).completeBaseName();
    const QString cfgPath  = QDir(markersBaseDirForCurrentVideo()).filePath(
        QStringLiteral("player_highlighter_%1.yaml")
            .arg(QDateTime::currentDateTime().toString("yyyyMMddHHmmss")));

    cv::FileStorage fs(cfgPath.toStdString(), cv::FileStorage::WRITE);
    if (!fs.isOpened()) return {};

    fs << "video_path"      << videoAbs.toStdString();
    fs << "player_csv_path" << playerCsvPath.toStdString();
    fs << "highlight_mode"  << "dim";
    fs << "background_dim"  << 0.30;
    fs << "draw_skeleton"   << 1;
    fs << "draw_bbox"       << 1;
    fs << "contour_thickness"      << 2;
    fs << "grabcut_iterations"     << 3;
    fs << "grabcut_margin"         << 24;
    fs << "joint_seed_radius"      << 4;
    fs << "keypoint_vis_threshold" << 0.35;
    fs << "enable_depth"  << 0;
    fs << "enable_zoom"   << 1;
    fs << "zoom_factor"   << 1.6;
    fs << "zoom_output_width"     << 960;
    fs << "zoom_output_height"    << 540;
    fs << "zoom_smoothing_alpha"  << 0.20;
    fs << "overlay_video_path" << makeOutPath(stem, "player_highlighter",
                                              "highlight", ".mp4").toStdString();
    fs << "zoom_video_path"    << makeOutPath(stem, "player_highlighter",
                                              "zoom",      ".mp4").toStdString();
    fs.release();
    return cfgPath;
}

void MainWindow::onGenerateDiscConfig()
{
    const QString markers = ensureMarkersSaved();
    if (markers.isEmpty()) return;
    const QString cfg = writeDiscTrackerConfig(markers);
    if (cfg.isEmpty()) { appendLog(tr("disc_tracker config generation failed.")); return; }
    m_lastDiscCfg = cfg;
    m_btnRunDisc->setEnabled(true);
    appendLog(tr("disc_tracker config: %1").arg(cfg));
}

void MainWindow::onGeneratePlayerConfig()
{
    const QString markers = ensureMarkersSaved();
    if (markers.isEmpty()) return;
    const QString cfg = writePlayerTrackerConfig(markers);
    if (cfg.isEmpty()) { appendLog(tr("player_tracker config generation failed.")); return; }
    m_lastPlayerCfg = cfg;
    m_btnRunPlayer->setEnabled(true);
    appendLog(tr("player_tracker config: %1").arg(cfg));
}

void MainWindow::onGenerateHighlighterConfig()
{
    if (m_lastSource.isEmpty()) {
        QMessageBox::information(this, tr("Subservices"),
            tr("Open a video first."));
        return;
    }
    const QString csv = findLatestPlayerCsv();
    if (csv.isEmpty()) {
        QMessageBox::information(this, tr("Subservices"),
            tr("No player_tracker CSV found yet.\n"
               "Run player_tracker first, then generate the highlighter config."));
        return;
    }
    const QString cfg = writePlayerHighlighterConfig(csv);
    if (cfg.isEmpty()) { appendLog(tr("player_highlighter config generation failed.")); return; }
    m_lastHighlighterCfg = cfg;
    m_btnRunHighlighter->setEnabled(true);
    appendLog(tr("player_highlighter config: %1 (csv=%2)").arg(cfg, csv));
}

void MainWindow::runSubservice(const QString &serviceName,
                               const QString &exePath,
                               const QString &configPath)
{
    if (m_subserviceProc && m_subserviceProc->state() != QProcess::NotRunning) {
        appendLog(tr("Another subservice (%1) is still running — wait for it to finish.")
                      .arg(m_runningServiceName));
        return;
    }
    if (exePath.isEmpty() || !QFile::exists(exePath)) {
        appendLog(tr("Could not locate %1 executable. Build the subservice first.")
                      .arg(serviceName));
        QMessageBox::warning(this, tr("Subservices"),
            tr("Could not locate %1.exe.\n\nLooked under build_mingw*, build/bin, "
               "and PATH. Build the subservice with cmake first.").arg(serviceName));
        return;
    }
    if (!QFile::exists(configPath)) {
        appendLog(tr("Config not found: %1").arg(configPath));
        return;
    }

    if (!m_subserviceProc) {
        m_subserviceProc = new QProcess(this);
        connect(m_subserviceProc, &QProcess::readyReadStandardOutput,
                this, &MainWindow::onSubserviceStdout);
        connect(m_subserviceProc, &QProcess::readyReadStandardError,
                this, &MainWindow::onSubserviceStderr);
        connect(m_subserviceProc,
                QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this](int code, QProcess::ExitStatus){ onSubserviceFinished(code); });
    }

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QStringList pathExtras;
    QDir root(QCoreApplication::applicationDirPath());
    for (int up = 0; up < 6; ++up) {
        const QString p = root.filePath(
            QStringLiteral("vcpkg_installed/x64-mingw-dynamic/bin"));
        if (QDir(p).exists()) { pathExtras << QDir::toNativeSeparators(p); break; }
        if (!root.cdUp()) break;
    }
    if (!pathExtras.isEmpty()) {
        const QString cur = env.value(QStringLiteral("PATH"));
        env.insert(QStringLiteral("PATH"),
                   pathExtras.join(QDir::listSeparator()) + QDir::listSeparator() + cur);
        m_subserviceProc->setProcessEnvironment(env);
    }

    m_runningServiceName = serviceName;
    appendLog(tr("Running %1: \"%2\" --config \"%3\"")
                  .arg(serviceName, exePath, configPath));
    m_btnRunDisc->setEnabled(false);
    m_btnRunPlayer->setEnabled(false);
    m_btnRunHighlighter->setEnabled(false);
    m_subserviceProc->setProgram(exePath);
    m_subserviceProc->setArguments({QStringLiteral("--config"), configPath});
    m_subserviceProc->start();
}

void MainWindow::onRunDiscTracker()
{
    runSubservice(QStringLiteral("disc_tracker"),
                  findSubserviceExe(QStringLiteral("disc_tracker")),
                  m_lastDiscCfg);
}

void MainWindow::onRunPlayerTracker()
{
    runSubservice(QStringLiteral("player_tracker"),
                  findSubserviceExe(QStringLiteral("player_tracker")),
                  m_lastPlayerCfg);
}

void MainWindow::onRunPlayerHighlighter()
{
    runSubservice(QStringLiteral("player_highlighter"),
                  findSubserviceExe(QStringLiteral("player_highlighter")),
                  m_lastHighlighterCfg);
}

void MainWindow::onSubserviceStdout()
{
    if (!m_subserviceProc) return;
    const QByteArray data = m_subserviceProc->readAllStandardOutput();
    if (m_subserviceLog) m_subserviceLog->appendPlainText(QString::fromLocal8Bit(data).trimmed());
}

void MainWindow::onSubserviceStderr()
{
    if (!m_subserviceProc) return;
    const QByteArray data = m_subserviceProc->readAllStandardError();
    if (m_subserviceLog) m_subserviceLog->appendPlainText(QString::fromLocal8Bit(data).trimmed());
}

void MainWindow::onSubserviceFinished(int exitCode)
{
    appendLog(tr("%1 finished (exit %2)")
                  .arg(m_runningServiceName).arg(exitCode));
    m_runningServiceName.clear();
    m_btnRunDisc->setEnabled(!m_lastDiscCfg.isEmpty());
    m_btnRunPlayer->setEnabled(!m_lastPlayerCfg.isEmpty());
    m_btnRunHighlighter->setEnabled(!m_lastHighlighterCfg.isEmpty());
}

// ---------- Subservices: stats / joint angles ----------

namespace {

// Returns the angle (degrees) at vertex `b` of the triangle (a, b, c).
// Returns NaN if any side has zero length.
double angleAt(double ax, double ay, double bx, double by, double cx, double cy)
{
    const double v1x = ax - bx, v1y = ay - by;
    const double v2x = cx - bx, v2y = cy - by;
    const double n1 = std::hypot(v1x, v1y);
    const double n2 = std::hypot(v2x, v2y);
    if (n1 < 1e-6 || n2 < 1e-6) return std::nan("");
    double c = (v1x * v2x + v1y * v2y) / (n1 * n2);
    if (c >  1.0) c =  1.0;
    if (c < -1.0) c = -1.0;
    return qRadiansToDegrees(std::acos(c));
}

}  // namespace

void MainWindow::onLoadStats()
{
    QString startDir;
    QDir root(QCoreApplication::applicationDirPath());
    for (int up = 0; up < 6; ++up) {
        const QString p = root.filePath(QStringLiteral("player_tracker/out"));
        if (QDir(p).exists()) { startDir = p; break; }
        if (!root.cdUp()) break;
    }
    if (startDir.isEmpty()) startDir = QDir::homePath();

    const QString path = QFileDialog::getOpenFileName(
        this, tr("Load player_tracker CSV"),
        startDir, tr("CSV files (*.csv)"));
    if (path.isEmpty()) return;
    loadJointAnglesFromCsv(path);
}

void MainWindow::loadJointAnglesFromCsv(const QString &csvPath)
{
    QFile file(csvPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Load stats"),
            tr("Cannot open CSV: %1").arg(csvPath));
        return;
    }
    QTextStream in(&file);
    const QString header = in.readLine();
    if (header.isEmpty()) {
        QMessageBox::warning(this, tr("Load stats"),
            tr("CSV is empty: %1").arg(csvPath));
        return;
    }
    const QStringList cols = header.split(QLatin1Char(','));
    QHash<QString, int> idx;
    for (int i = 0; i < cols.size(); ++i) idx.insert(cols.at(i).trimmed(), i);

    auto need = [&](const QString &c) {
        if (!idx.contains(c)) {
            throw std::runtime_error(("CSV missing column: " + c).toStdString());
        }
        return idx.value(c);
    };

    // For each priority joint we compute one angle from a triple of joints.
    // (label, A, B, C) — angle is the angle at B in the triangle A-B-C.
    struct AngleSpec { QString label, a, b, c; };
    const QVector<AngleSpec> specs = {
        {tr("Left elbow"),     "left_shoulder",  "left_elbow",     "left_wrist"},
        {tr("Right elbow"),    "right_shoulder", "right_elbow",    "right_wrist"},
        {tr("Left shoulder"),  "left_hip",       "left_shoulder",  "left_elbow"},
        {tr("Right shoulder"), "right_hip",      "right_shoulder", "right_elbow"},
        {tr("Left knee"),      "left_hip",       "left_knee",      "left_ankle"},
        {tr("Right knee"),     "right_hip",      "right_knee",     "right_ankle"},
    };

    struct Cols { int ax, ay, av, bx, by, bv, cx, cy, cv; };
    QVector<Cols> colsList;
    colsList.reserve(specs.size());
    try {
        for (const auto &s : specs) {
            colsList.append({
                need(s.a + "_x"), need(s.a + "_y"), need(s.a + "_v"),
                need(s.b + "_x"), need(s.b + "_y"), need(s.b + "_v"),
                need(s.c + "_x"), need(s.c + "_y"), need(s.c + "_v"),
            });
        }
    } catch (const std::exception &e) {
        QMessageBox::warning(this, tr("Load stats"),
            tr("This CSV doesn't look like a player_tracker output: %1")
                .arg(QString::fromUtf8(e.what())));
        return;
    }

    const int hasPoseCol = idx.value("has_pose", -1);
    constexpr float kVisFloor = 0.35f;

    QVector<int>    hits(specs.size(), 0);
    QVector<double> sum(specs.size(), 0.0);
    QVector<double> mn(specs.size(),  std::numeric_limits<double>::max());
    QVector<double> mx(specs.size(), -std::numeric_limits<double>::max());

    int totalRows = 0;
    while (!in.atEnd()) {
        const QString line = in.readLine();
        if (line.isEmpty()) continue;
        const QStringList cells = line.split(QLatin1Char(','));
        if (cells.size() < cols.size()) continue;
        ++totalRows;
        if (hasPoseCol >= 0 && cells.value(hasPoseCol).toInt() == 0) continue;

        for (int i = 0; i < specs.size(); ++i) {
            const Cols &c = colsList[i];
            const float va = cells.value(c.av).toFloat();
            const float vb = cells.value(c.bv).toFloat();
            const float vc = cells.value(c.cv).toFloat();
            if (va < kVisFloor || vb < kVisFloor || vc < kVisFloor) continue;
            const double ang = angleAt(
                cells.value(c.ax).toDouble(), cells.value(c.ay).toDouble(),
                cells.value(c.bx).toDouble(), cells.value(c.by).toDouble(),
                cells.value(c.cx).toDouble(), cells.value(c.cy).toDouble());
            if (std::isnan(ang)) continue;
            ++hits[i];
            sum[i] += ang;
            if (ang < mn[i]) mn[i] = ang;
            if (ang > mx[i]) mx[i] = ang;
        }
    }

    if (m_poseJointList) m_poseJointList->clear();
    for (int i = 0; i < specs.size(); ++i) {
        QString text;
        if (hits[i] == 0) {
            text = QStringLiteral("%1: —").arg(specs[i].label);
        } else {
            const double mean = sum[i] / hits[i];
            text = QStringLiteral("%1: mean %2°  [min %3° / max %4°]  (%5 frames)")
                       .arg(specs[i].label)
                       .arg(mean,   0, 'f', 1)
                       .arg(mn[i],  0, 'f', 1)
                       .arg(mx[i],  0, 'f', 1)
                       .arg(hits[i]);
        }
        if (m_poseJointList) m_poseJointList->addItem(text);
    }
    appendLog(tr("Joint angles computed from %1 (%2 rows).")
                  .arg(QFileInfo(csvPath).fileName()).arg(totalRows));
    statusBar()->showMessage(tr("Loaded joint-angle stats from %1")
                                 .arg(QFileInfo(csvPath).fileName()),
                             5000);
}

// ---------- Result overlays: file finders ----------

namespace {

// Walks up to 6 levels above applicationDirPath looking for a sub-tree
// matching `subDir`, then returns the most-recent file matching any of
// the glob filters. Empty string if not found.
QString findLatestUnder(const QString &subDir, const QStringList &filters)
{
    QDir root(QCoreApplication::applicationDirPath());
    for (int up = 0; up < 6; ++up) {
        const QString dir = root.filePath(subDir);
        QDir d(dir);
        if (d.exists()) {
            QFileInfoList files = d.entryInfoList(filters,
                QDir::Files | QDir::NoDotAndDotDot, QDir::Time);
            if (!files.isEmpty()) return files.first().absoluteFilePath();
        }
        if (!root.cdUp()) break;
    }
    return {};
}

}  // namespace

QString MainWindow::findLatestDiscCsv() const
{
    if (m_lastSource.isEmpty()) return {};
    const QString stem = QFileInfo(m_lastSource).completeBaseName();
    return findLatestUnder(QStringLiteral("disc_tracker/out"), {
        stem + QStringLiteral("_*.csv"),
        QStringLiteral("hole*_anchored_path_*.csv"),
        QStringLiteral("*_anchored_path_*.csv"),
        QStringLiteral("*_path_*.csv"),
    });
}

QString MainWindow::findLatestHighlightVideo() const
{
    if (m_lastSource.isEmpty()) return {};
    const QString stem = QFileInfo(m_lastSource).completeBaseName();
    return findLatestUnder(QStringLiteral("player_highlighter/out"), {
        stem + QStringLiteral("_highlight_*.mp4"),
        QStringLiteral("hole*_highlight_*.mp4"),
        QStringLiteral("*_highlight_*.mp4"),
    });
}

QString MainWindow::findLatestZoomVideo() const
{
    if (m_lastSource.isEmpty()) return {};
    const QString stem = QFileInfo(m_lastSource).completeBaseName();
    return findLatestUnder(QStringLiteral("player_highlighter/out"), {
        stem + QStringLiteral("_zoom_*.mp4"),
        QStringLiteral("hole*_zoom_*.mp4"),
        QStringLiteral("*_zoom_*.mp4"),
    });
}

// ---------- Result overlays: CSV loaders ----------

bool MainWindow::loadDiscTrackForCurrentVideo()
{
    m_discTrack.clear();
    const QString csv = findLatestDiscCsv();
    if (csv.isEmpty()) {
        appendLog(tr("No disc_tracker CSV found for the current video."));
        return false;
    }

    QFile f(csv);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        appendLog(tr("Cannot open disc CSV: %1").arg(csv));
        return false;
    }
    QTextStream in(&f);
    const QString header = in.readLine();
    const QStringList cols = header.split(QLatin1Char(','));
    QHash<QString, int> idx;
    for (int i = 0; i < cols.size(); ++i) idx.insert(cols.at(i).trimmed(), i);

    auto col = [&](const QString &n, int fallback = -1) {
        return idx.value(n, fallback);
    };
    const int cFrame = col("frame_idx");
    const int cTime  = col("time_ms");
    const int cBx    = col("x"),  cBy = col("y"), cBw = col("w"), cBh = col("h");
    const int cCx    = col("cx"), cCy = col("cy");
    const int cSrc   = col("source");
    if (cFrame < 0 || cTime < 0 || cCx < 0 || cCy < 0) {
        appendLog(tr("disc_tracker CSV missing required columns."));
        return false;
    }

    while (!in.atEnd()) {
        const QString line = in.readLine();
        if (line.isEmpty()) continue;
        const QStringList c = line.split(QLatin1Char(','));
        if (c.size() < cols.size()) continue;
        DiscRow r;
        r.frameIdx = c.value(cFrame).toInt();
        r.timeMs   = c.value(cTime).toDouble();
        r.cx       = c.value(cCx).toFloat();
        r.cy       = c.value(cCy).toFloat();
        if (cBx >= 0) r.bboxX = c.value(cBx).toFloat();
        if (cBy >= 0) r.bboxY = c.value(cBy).toFloat();
        if (cBw >= 0) r.bboxW = c.value(cBw).toFloat();
        if (cBh >= 0) r.bboxH = c.value(cBh).toFloat();
        if (cSrc >= 0) r.source = c.value(cSrc).trimmed();
        m_discTrack.push_back(r);
    }
    appendLog(tr("Loaded disc track: %1 rows from %2")
                  .arg(m_discTrack.size()).arg(QFileInfo(csv).fileName()));
    return !m_discTrack.isEmpty();
}

bool MainWindow::loadSkeletonTrackForCurrentVideo()
{
    m_skelTrack.clear();
    if (m_lastSource.isEmpty()) return false;
    const QString csv = findLatestPlayerCsv();
    if (csv.isEmpty()) {
        appendLog(tr("No player_tracker CSV found for the current video."));
        return false;
    }

    QFile f(csv);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        appendLog(tr("Cannot open player CSV: %1").arg(csv));
        return false;
    }
    QTextStream in(&f);
    const QString header = in.readLine();
    const QStringList cols = header.split(QLatin1Char(','));
    QHash<QString, int> idx;
    for (int i = 0; i < cols.size(); ++i) idx.insert(cols.at(i).trimmed(), i);

    static const char *kKpNames[17] = {
        "nose", "left_eye", "right_eye", "left_ear", "right_ear",
        "left_shoulder", "right_shoulder", "left_elbow", "right_elbow",
        "left_wrist",    "right_wrist",
        "left_hip",      "right_hip",
        "left_knee",     "right_knee",
        "left_ankle",    "right_ankle",
    };

    int cFrame = idx.value("frame_idx", -1);
    int cTime  = idx.value("time_ms",   -1);
    int cHas   = idx.value("has_pose",  -1);
    int cBx    = idx.value("bbox_x",    -1);
    int cBy    = idx.value("bbox_y",    -1);
    int cBw    = idx.value("bbox_w",    -1);
    int cBh    = idx.value("bbox_h",    -1);
    if (cFrame < 0 || cTime < 0 || cHas < 0 || cBx < 0) {
        appendLog(tr("player_tracker CSV missing required columns."));
        return false;
    }
    int cKpX[17], cKpY[17], cKpV[17];
    for (int i = 0; i < 17; ++i) {
        cKpX[i] = idx.value(QString::fromLatin1(kKpNames[i]) + "_x", -1);
        cKpY[i] = idx.value(QString::fromLatin1(kKpNames[i]) + "_y", -1);
        cKpV[i] = idx.value(QString::fromLatin1(kKpNames[i]) + "_v", -1);
        if (cKpX[i] < 0 || cKpY[i] < 0 || cKpV[i] < 0) {
            appendLog(tr("player CSV missing keypoint columns for %1")
                          .arg(QString::fromLatin1(kKpNames[i])));
            return false;
        }
    }

    while (!in.atEnd()) {
        const QString line = in.readLine();
        if (line.isEmpty()) continue;
        const QStringList c = line.split(QLatin1Char(','));
        if (c.size() < cols.size()) continue;
        SkelRow r;
        r.frameIdx = c.value(cFrame).toInt();
        r.timeMs   = c.value(cTime).toDouble();
        r.hasPose  = (c.value(cHas).toInt() != 0);
        r.bboxX    = c.value(cBx).toFloat();
        r.bboxY    = c.value(cBy).toFloat();
        r.bboxW    = c.value(cBw).toFloat();
        r.bboxH    = c.value(cBh).toFloat();
        for (int i = 0; i < 17; ++i) {
            r.kpX[i] = c.value(cKpX[i]).toFloat();
            r.kpY[i] = c.value(cKpY[i]).toFloat();
            r.kpV[i] = c.value(cKpV[i]).toFloat();
        }
        m_skelTrack.push_back(r);
    }
    appendLog(tr("Loaded skeleton track: %1 rows from %2")
                  .arg(m_skelTrack.size()).arg(QFileInfo(csv).fileName()));
    return !m_skelTrack.isEmpty();
}

// ---------- Result overlays: row matching ----------

namespace {

// Generic closest-time lookup. Returns -1 if nothing within `tolMs`.
template <typename Row>
int closestRow(const QVector<Row> &rows, double playbackMs, double tolMs)
{
    if (rows.isEmpty()) return -1;
    int   best = -1;
    double bestDt = std::numeric_limits<double>::max();
    // Linear scan — typical track length is <500 rows; binary search not worth it.
    for (int i = 0; i < rows.size(); ++i) {
        const double dt = std::abs(rows[i].timeMs - playbackMs);
        if (dt < bestDt) { bestDt = dt; best = i; }
    }
    return (bestDt <= tolMs) ? best : -1;
}

}  // namespace

int MainWindow::findClosestDiscRow(double playbackMs) const
{
    return closestRow(m_discTrack, playbackMs, /*tolMs=*/100.0);
}

int MainWindow::findClosestSkelRow(double playbackMs) const
{
    return closestRow(m_skelTrack, playbackMs, /*tolMs=*/100.0);
}

// ---------- Result overlays: drawing ----------

void MainWindow::drawDiscOverlayOnPixmap(QPixmap &pix) const
{
    if (m_discTrack.isEmpty() || pix.isNull() || m_currentFrame.isNull()) return;
    const double sx = double(pix.width())  / m_currentFrame.width();
    const double sy = double(pix.height()) / m_currentFrame.height();

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);

    const double playbackMs = m_positionSec * 1000.0;
    const int hereIdx = findClosestDiscRow(playbackMs);

    // Trail: every row up to and including the current playback time.
    QVector<QPointF> trail;
    trail.reserve(m_discTrack.size());
    for (const DiscRow &r : m_discTrack) {
        if (r.timeMs > playbackMs + 1.0) break;
        if (r.source == QLatin1String("lost")) continue;
        trail.append(QPointF(r.cx * sx, r.cy * sy));
    }
    if (trail.size() >= 2) {
        QPen pen(QColor(0, 200, 255, 220));
        pen.setWidth(2);
        p.setPen(pen);
        for (int i = 1; i < trail.size(); ++i) p.drawLine(trail[i - 1], trail[i]);
    }

    // Current bbox + dot, color-coded by source.
    if (hereIdx >= 0) {
        const DiscRow &r = m_discTrack[hereIdx];
        QColor c(0, 220, 60);                       // detector = green
        if (r.source == QLatin1String("anchor"))         c = QColor(255, 200, 0);
        else if (r.source == QLatin1String("kalman_predict")) c = QColor(0, 165, 255);
        else if (r.source == QLatin1String("lost"))           c = QColor(180, 60, 60);

        if (r.bboxW > 0 && r.bboxH > 0) {
            QPen rp(c); rp.setWidth(2); p.setPen(rp);
            p.drawRect(QRectF(r.bboxX * sx, r.bboxY * sy,
                              r.bboxW * sx, r.bboxH * sy));
        }
        p.setBrush(c);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(r.cx * sx, r.cy * sy), 4.0, 4.0);
    }
    p.end();
}

void MainWindow::drawSkeletonOverlayOnPixmap(QPixmap &pix) const
{
    if (m_skelTrack.isEmpty() || pix.isNull() || m_currentFrame.isNull()) return;
    const double playbackMs = m_positionSec * 1000.0;
    const int hereIdx = findClosestSkelRow(playbackMs);
    if (hereIdx < 0) return;
    const SkelRow &r = m_skelTrack[hereIdx];
    if (!r.hasPose) return;

    const double sx = double(pix.width())  / m_currentFrame.width();
    const double sy = double(pix.height()) / m_currentFrame.height();

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);

    // bbox
    if (r.bboxW > 0 && r.bboxH > 0) {
        QPen bp(QColor(60, 220, 60)); bp.setWidth(2); p.setPen(bp);
        p.drawRect(QRectF(r.bboxX * sx, r.bboxY * sy,
                          r.bboxW * sx, r.bboxH * sy));
    }

    // skeleton edges (COCO-17)
    static const int kEdges[][2] = {
        {5, 7}, {7, 9}, {6, 8}, {8, 10},        // arms
        {5, 6}, {5, 11}, {6, 12}, {11, 12},     // shoulders/torso
        {11, 13}, {13, 15}, {12, 14}, {14, 16}, // legs
        {0, 1}, {0, 2}, {1, 3}, {2, 4},         // head
    };
    constexpr float kVisFloor = 0.35f;
    QPen ep(QColor(180, 180, 60, 220)); ep.setWidth(2); p.setPen(ep);
    for (const auto &e : kEdges) {
        const int a = e[0], b = e[1];
        if (r.kpV[a] < kVisFloor || r.kpV[b] < kVisFloor) continue;
        p.drawLine(QPointF(r.kpX[a] * sx, r.kpY[a] * sy),
                   QPointF(r.kpX[b] * sx, r.kpY[b] * sy));
    }

    // dots: priority joints highlighted; rest small.
    static const int kPriority[] = {5, 6, 7, 8, 13, 14};
    auto isPriority = [&](int kp) {
        for (int q : kPriority) if (q == kp) return true;
        return false;
    };
    for (int i = 0; i < 17; ++i) {
        if (r.kpV[i] < kVisFloor) continue;
        const QPointF c(r.kpX[i] * sx, r.kpY[i] * sy);
        if (isPriority(i)) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 0, 255));
            p.drawEllipse(c, 5.0, 5.0);
            QPen halo(QColor(255, 255, 255)); halo.setWidth(1);
            p.setPen(halo); p.setBrush(Qt::NoBrush);
            p.drawEllipse(c, 7.0, 7.0);
        } else {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(80, 200, 255, 220));
            p.drawEllipse(c, 3.0, 3.0);
        }
    }
    p.end();
}

// ---------- Result overlays: slots ----------

void MainWindow::onToggleDiscOverlay(bool checked)
{
    if (checked) {
        if (m_discTrack.isEmpty() && !loadDiscTrackForCurrentVideo()) {
            QSignalBlocker b(m_btnShowDiscTrack);
            m_btnShowDiscTrack->setChecked(false);
            QMessageBox::information(this, tr("Result overlays"),
                tr("No disc_tracker CSV found for this video.\n"
                   "Run disc_tracker first."));
            return;
        }
    }
    m_showDiscTrack = checked;
    renderFrame();
}

void MainWindow::onToggleSkeletonOverlay(bool checked)
{
    if (checked) {
        if (m_skelTrack.isEmpty() && !loadSkeletonTrackForCurrentVideo()) {
            QSignalBlocker b(m_btnShowSkeleton);
            m_btnShowSkeleton->setChecked(false);
            QMessageBox::information(this, tr("Result overlays"),
                tr("No player_tracker CSV found for this video.\n"
                   "Run player_tracker first."));
            return;
        }
    }
    m_showSkeleton = checked;
    renderFrame();
}

void MainWindow::onSwitchSourceOriginal()
{
    if (m_originalSourcePath.isEmpty()) return;
    if (m_originalSourcePath == m_lastSource) return;
    m_swappingSource = true;
    startFile(m_originalSourcePath);
    m_swappingSource = false;
    appendLog(tr("Switched playback to original: %1").arg(m_originalSourcePath));
}

void MainWindow::onSwitchSourceHighlight()
{
    if (m_originalSourcePath.isEmpty() && !m_lastSource.isEmpty())
        m_originalSourcePath = m_lastSource;
    const QString p = findLatestHighlightVideo();
    if (p.isEmpty()) {
        QMessageBox::information(this, tr("Result overlays"),
            tr("No highlighter overlay video found for this video.\n"
               "Run player_highlighter first."));
        return;
    }
    m_swappingSource = true;
    startFile(p);
    m_swappingSource = false;
    appendLog(tr("Switched playback to highlight: %1").arg(p));
}

void MainWindow::onSwitchSourceZoom()
{
    if (m_originalSourcePath.isEmpty() && !m_lastSource.isEmpty())
        m_originalSourcePath = m_lastSource;
    const QString p = findLatestZoomVideo();
    if (p.isEmpty()) {
        QMessageBox::information(this, tr("Result overlays"),
            tr("No highlighter zoom video found for this video.\n"
               "Run player_highlighter first."));
        return;
    }
    m_swappingSource = true;
    startFile(p);
    m_swappingSource = false;
    appendLog(tr("Switched playback to zoom: %1").arg(p));
}

void MainWindow::onThemeSelected(QAction *act)
{
    if (!act) return;
    const Theme t = static_cast<Theme>(act->data().toInt());
    applyTheme(t);
    saveTheme(t);
}

MainWindow::Theme MainWindow::loadSavedTheme()
{
    QSettings s;
    const int v = s.value(kThemeKey, int(Theme::System)).toInt();
    if (v < int(Theme::System) || v > int(Theme::SolarizedLight))
        return Theme::System;
    return static_cast<Theme>(v);
}

void MainWindow::saveTheme(Theme theme)
{
    QSettings s;
    s.setValue(kThemeKey, int(theme));
}

void MainWindow::applyTheme(Theme theme)
{
    auto *app = qobject_cast<QApplication *>(QCoreApplication::instance());
    if (!app) return;

    if (!g_defaultsCaptured) {
        g_defaultStyleName  = app->style() ? app->style()->objectName() : QString();
        g_defaultPalette    = app->palette();
        g_defaultsCaptured  = true;
    }

    switch (theme) {
    case Theme::System: {
        if (!g_defaultStyleName.isEmpty()) {
            if (auto *style = QStyleFactory::create(g_defaultStyleName))
                app->setStyle(style);
        }
        app->setStyleSheet(QString());
        app->setPalette(g_defaultPalette);
        break;
    }
    case Theme::Dark: {
        if (auto *style = QStyleFactory::create(QStringLiteral("Fusion")))
            app->setStyle(style);
        QPalette p;
        p.setColor(QPalette::Window,          QColor(53, 53, 53));
        p.setColor(QPalette::WindowText,      Qt::white);
        p.setColor(QPalette::Base,            QColor(35, 35, 35));
        p.setColor(QPalette::AlternateBase,   QColor(53, 53, 53));
        p.setColor(QPalette::ToolTipBase,     QColor(53, 53, 53));
        p.setColor(QPalette::ToolTipText,     Qt::white);
        p.setColor(QPalette::Text,            Qt::white);
        p.setColor(QPalette::Button,          QColor(53, 53, 53));
        p.setColor(QPalette::ButtonText,      Qt::white);
        p.setColor(QPalette::BrightText,      Qt::red);
        p.setColor(QPalette::Link,            QColor(42, 130, 218));
        p.setColor(QPalette::Highlight,       QColor(42, 130, 218));
        p.setColor(QPalette::HighlightedText, Qt::black);
        p.setColor(QPalette::Disabled, QPalette::Text,       QColor(127, 127, 127));
        p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(127, 127, 127));
        p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(127, 127, 127));
        app->setPalette(p);
        app->setStyleSheet(QStringLiteral(
            "QToolTip { color: #ffffff; background-color: #2a82da; "
            "border: 1px solid white; }"));
        break;
    }
    case Theme::SolarizedLight: {
        if (auto *style = QStyleFactory::create(QStringLiteral("Fusion")))
            app->setStyle(style);
        QPalette p;
        p.setColor(QPalette::Window,          QColor(0xee, 0xe8, 0xd5));
        p.setColor(QPalette::WindowText,      QColor(0x58, 0x6e, 0x75));
        p.setColor(QPalette::Base,            QColor(0xfd, 0xf6, 0xe3));
        p.setColor(QPalette::AlternateBase,   QColor(0xee, 0xe8, 0xd5));
        p.setColor(QPalette::ToolTipBase,     QColor(0xfd, 0xf6, 0xe3));
        p.setColor(QPalette::ToolTipText,     QColor(0x58, 0x6e, 0x75));
        p.setColor(QPalette::Text,            QColor(0x58, 0x6e, 0x75));
        p.setColor(QPalette::Button,          QColor(0xee, 0xe8, 0xd5));
        p.setColor(QPalette::ButtonText,      QColor(0x58, 0x6e, 0x75));
        p.setColor(QPalette::BrightText,      QColor(0xdc, 0x32, 0x2f));
        p.setColor(QPalette::Link,            QColor(0x26, 0x8b, 0xd2));
        p.setColor(QPalette::Highlight,       QColor(0xb5, 0x89, 0x00));
        p.setColor(QPalette::HighlightedText, QColor(0xfd, 0xf6, 0xe3));
        p.setColor(QPalette::Disabled, QPalette::Text,       QColor(0x93, 0xa1, 0xa1));
        p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x93, 0xa1, 0xa1));
        p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x93, 0xa1, 0xa1));
        app->setPalette(p);
        app->setStyleSheet(QString());
        break;
    }
    }
}
