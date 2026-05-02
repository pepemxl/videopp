#include "MainWindow.h"
#include <cmath>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QScrollArea>
#include <QSettings>
#include <QSignalBlocker>
#include <QSlider>
#include <QTime>
#include <QVBoxLayout>
#include <QWidget>
#include <QImage>
#include <QPixmap>
#include <QWheelEvent>

namespace {
constexpr double kSeekStepSeconds = 5.0;
constexpr double kSpeedStep       = 1.25;   // multiplicative
constexpr double kSpeedMin        = 0.25;
constexpr double kSpeedMax        = 4.0;
constexpr double kZoomStep        = 1.15;   // multiplicative
constexpr double kZoomMin         = 0.25;
constexpr double kZoomMax         = 6.0;
constexpr int    kMaxRecent       = 10;
constexpr int    kSliderMaxMs     = 1'000'000'000;  // ~11.5 days, safe upper bound
const QString kRecentKey      = QStringLiteral("recentVideos");
const QString kLastOpenDirKey = QStringLiteral("lastOpenDir");
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_processor(new VideoProcessor(this))
{
    setWindowTitle(tr("Pepe Golf Disc Analyzer"));

    m_videoLabel = new QLabel(tr("Video display"));
    m_videoLabel->setAlignment(Qt::AlignCenter);
    m_videoLabel->setMinimumSize(640, 480);
    m_videoLabel->setBackgroundRole(QPalette::Dark);
    m_videoLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    m_scrollArea = new QScrollArea;
    m_scrollArea->setWidget(m_videoLabel);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setAlignment(Qt::AlignCenter);
    m_scrollArea->viewport()->installEventFilter(this);
    m_scrollArea->installEventFilter(this);

    m_btnStartFile = new QPushButton(tr("Start from file"));
    m_btnStartCam  = new QPushButton(tr("Start from camera"));
    m_btnStop      = new QPushButton(tr("Stop"));
    m_btnRestart   = new QPushButton(tr("Restart"));
    m_btnBackward  = new QPushButton(tr("<< -5s"));
    m_btnPlayPause = new QPushButton(tr("Pause"));
    m_btnForward   = new QPushButton(tr("+5s >>"));
    m_btnRecord    = new QPushButton(tr("Record"));
    m_btnRecord->setCheckable(true);
    m_formatCombo  = new QComboBox;
    m_formatCombo->addItems({QStringLiteral("avi"),
                             QStringLiteral("mp4"),
                             QStringLiteral("mkv")});
    m_formatCombo->setToolTip(tr("Recording container format"));
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
    sourceRow->addWidget(m_btnStartFile);
    sourceRow->addWidget(m_btnStartCam);
    sourceRow->addWidget(m_btnStop);
    sourceRow->addWidget(m_btnRestart);

    auto playbackRow = new QHBoxLayout;
    playbackRow->addWidget(m_btnBackward);
    playbackRow->addWidget(m_btnPlayPause);
    playbackRow->addWidget(m_btnForward);
    playbackRow->addWidget(m_btnRecord);
    playbackRow->addWidget(m_formatCombo);
    playbackRow->addStretch();
    playbackRow->addWidget(m_speedLabel);

    auto layout = new QVBoxLayout;
    layout->addWidget(m_scrollArea, 1);
    layout->addLayout(seekRow);
    layout->addLayout(sourceRow);
    layout->addLayout(playbackRow);

    auto central = new QWidget;
    central->setLayout(layout);
    setCentralWidget(central);

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

    // ----- Filters menu -----
    QMenu *filtersMenu = menuBar()->addMenu(tr("F&ilters"));
    m_actGrayscale = filtersMenu->addAction(tr("&Grayscale"));
    m_actGrayscale->setCheckable(true);
    m_actGrayscale->setChecked(false);
    m_actBlur = filtersMenu->addAction(tr("Gaussian &Blur"));
    m_actBlur->setCheckable(true);
    m_actBlur->setChecked(false);

    connect(m_actGrayscale, &QAction::toggled,
            m_processor, &VideoProcessor::setGrayscaleEnabled);
    connect(m_actBlur, &QAction::toggled,
            m_processor, &VideoProcessor::setBlurEnabled);

    // ----- Buttons -----
    connect(m_btnStartFile, &QPushButton::clicked, this, &MainWindow::onStartFromFile);
    connect(m_btnStartCam,  &QPushButton::clicked, this, &MainWindow::onStartFromCamera);
    connect(m_btnStop,      &QPushButton::clicked, this, &MainWindow::onStop);
    connect(m_btnRestart,   &QPushButton::clicked, this, &MainWindow::onRestart);
    connect(m_btnPlayPause, &QPushButton::clicked, this, &MainWindow::onPlayPause);
    connect(m_btnForward,   &QPushButton::clicked, this, &MainWindow::onSeekForward);
    connect(m_btnBackward,  &QPushButton::clicked, this, &MainWindow::onSeekBackward);
    connect(m_btnRecord,    &QPushButton::toggled, this, &MainWindow::onToggleRecord);

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
            if (dy > 0) onZoomIn();
            else        onZoomOut();
            return true;  // consume — don't scroll the area
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::onFrameReady(const cv::Mat &frame)
{
    if (frame.empty()) return;

    if (frame.channels() == 3) {
        cv::Mat rgb;
        cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);
        m_currentFrame = QImage(rgb.data, rgb.cols, rgb.rows,
                                static_cast<int>(rgb.step),
                                QImage::Format_RGB888).copy();
    } else if (frame.channels() == 1) {
        m_currentFrame = QImage(frame.data, frame.cols, frame.rows,
                                static_cast<int>(frame.step),
                                QImage::Format_Grayscale8).copy();
    } else {
        return;
    }

    renderFrame();
}

void MainWindow::renderFrame()
{
    if (m_currentFrame.isNull()) return;

    const QSize viewport = m_scrollArea->viewport()->size();
    QSize fit = m_currentFrame.size().scaled(viewport, Qt::KeepAspectRatio);
    QSize target = fit * m_zoom;
    if (target.width() < 1 || target.height() < 1) return;

    QPixmap pix = QPixmap::fromImage(m_currentFrame).scaled(
        target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_videoLabel->setPixmap(pix);
    m_videoLabel->resize(pix.size());
}

void MainWindow::startFile(const QString &path, double startSec)
{
    if (m_processor->isRunning()) {
        m_processor->stopProcessing();
    }
    m_processor->setSource(path, VideoProcessor::FromFile);
    m_processor->setStartPositionSec(startSec);
    m_lastSource = path;
    m_lastPosSec = startSec;
    m_isFileSource = true;
    m_durationSec = 0.0;
    m_positionSec = startSec;
    m_btnPlayPause->setText(tr("Pause"));
    {
        QSignalBlocker b(m_seekSlider);
        m_seekSlider->setRange(0, 0);
        m_seekSlider->setValue(0);
    }
    updateTimeLabel();
    m_processor->start();
    addToRecent(path);
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
    m_btnPlayPause->setText(nowPaused ? tr("Play") : tr("Pause"));
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
    double z = m_zoom * kZoomStep;
    if (z > kZoomMax) z = kZoomMax;
    m_zoom = z;
    renderFrame();
}

void MainWindow::onZoomOut()
{
    double z = m_zoom / kZoomStep;
    if (z < kZoomMin) z = kZoomMin;
    m_zoom = z;
    renderFrame();
}

void MainWindow::onResetZoom()
{
    m_zoom = 1.0;
    renderFrame();
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
    m_btnRecord->setText(recording ? tr("Stop Recording") : tr("Record"));
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
    m_btnPlayPause->setText(tr("Play"));
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
    if (canResume) m_btnPlayPause->setText(tr("Play"));
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
