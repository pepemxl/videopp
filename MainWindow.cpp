#include "MainWindow.h"
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
const QString kRecentKey = QStringLiteral("recentVideos");
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
    m_btnBackward  = new QPushButton(tr("<< -5s"));
    m_btnPlayPause = new QPushButton(tr("Pause"));
    m_btnForward   = new QPushButton(tr("+5s >>"));
    m_speedLabel   = new QLabel(tr("Speed: 1.00x"));

    auto sourceRow = new QHBoxLayout;
    sourceRow->addWidget(m_btnStartFile);
    sourceRow->addWidget(m_btnStartCam);
    sourceRow->addWidget(m_btnStop);

    auto playbackRow = new QHBoxLayout;
    playbackRow->addWidget(m_btnBackward);
    playbackRow->addWidget(m_btnPlayPause);
    playbackRow->addWidget(m_btnForward);
    playbackRow->addStretch();
    playbackRow->addWidget(m_speedLabel);

    auto layout = new QVBoxLayout;
    layout->addWidget(m_scrollArea, 1);
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
    playbackMenu->addSeparator();
    QAction *actSpeedUp = playbackMenu->addAction(tr("Speed &Up"));
    actSpeedUp->setShortcut(QKeySequence(tr("Ctrl+Right")));
    QAction *actSpeedDown = playbackMenu->addAction(tr("Speed &Down"));
    actSpeedDown->setShortcut(QKeySequence(tr("Ctrl+Left")));
    QAction *actResetSpeed = playbackMenu->addAction(tr("&Reset Speed"));
    actResetSpeed->setShortcut(QKeySequence(tr("Ctrl+0")));

    connect(actPlayPause,  &QAction::triggered, this, &MainWindow::onPlayPause);
    connect(actStop,       &QAction::triggered, this, &MainWindow::onStop);
    connect(actSpeedUp,    &QAction::triggered, this, &MainWindow::onSpeedUp);
    connect(actSpeedDown,  &QAction::triggered, this, &MainWindow::onSpeedDown);
    connect(actResetSpeed, &QAction::triggered, this, &MainWindow::onResetSpeed);

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
    connect(m_btnPlayPause, &QPushButton::clicked, this, &MainWindow::onPlayPause);
    connect(m_btnForward,   &QPushButton::clicked, this, &MainWindow::onSeekForward);
    connect(m_btnBackward,  &QPushButton::clicked, this, &MainWindow::onSeekBackward);

    connect(m_processor, &VideoProcessor::frameReady,
            this, &MainWindow::onFrameReady);
    connect(m_processor, &VideoProcessor::finished,
            this, &MainWindow::onProcessorFinished);
    connect(m_processor, &VideoProcessor::error,
            this, &MainWindow::onError);

    loadRecent();
    rebuildRecentMenu();
    updatePlaybackControls();
    updateSpeedLabel();
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
    m_btnPlayPause->setText(tr("Pause"));
    m_processor->start();
    addToRecent(path);
    updatePlaybackControls();
}

void MainWindow::onStartFromFile()
{
    const QString file = QFileDialog::getOpenFileName(
        this, tr("Open video file"), QDir::homePath(),
        tr("Video (*.mp4 *.avi *.mkv *.mov *.webm)"));
    if (!file.isEmpty()) {
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
    m_processor->start();
    updatePlaybackControls();
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
    const bool fileActive = m_isFileSource && m_processor->isRunning();
    const bool canResume  = !m_processor->isRunning() && !m_lastSource.isEmpty();
    m_btnPlayPause->setEnabled(fileActive || canResume);
    m_btnForward->setEnabled(fileActive);
    m_btnBackward->setEnabled(fileActive);
    if (canResume) m_btnPlayPause->setText(tr("Play"));
}

void MainWindow::updateSpeedLabel()
{
    m_speedLabel->setText(tr("Speed: %1x").arg(m_processor->speed(), 0, 'f', 2));
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
