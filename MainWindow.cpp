#include "MainWindow.h"
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QWidget>
#include <QImage>
#include <QPixmap>

namespace {
constexpr double kSeekStepSeconds = 5.0;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_processor(new VideoProcessor(this))
{
    m_videoLabel = new QLabel("Video display");
    m_videoLabel->setAlignment(Qt::AlignCenter);
    m_videoLabel->setMinimumSize(640, 480);

    m_btnStartFile = new QPushButton("Start from file");
    m_btnStartCam  = new QPushButton("Start from camera");
    m_btnStop      = new QPushButton("Stop");
    m_btnBackward  = new QPushButton("<< -5s");
    m_btnPlayPause = new QPushButton("Pause");
    m_btnForward   = new QPushButton("+5s >>");

    auto sourceRow = new QHBoxLayout;
    sourceRow->addWidget(m_btnStartFile);
    sourceRow->addWidget(m_btnStartCam);
    sourceRow->addWidget(m_btnStop);

    auto playbackRow = new QHBoxLayout;
    playbackRow->addWidget(m_btnBackward);
    playbackRow->addWidget(m_btnPlayPause);
    playbackRow->addWidget(m_btnForward);

    auto layout = new QVBoxLayout;
    layout->addWidget(m_videoLabel);
    layout->addLayout(sourceRow);
    layout->addLayout(playbackRow);

    auto central = new QWidget;
    central->setLayout(layout);
    setCentralWidget(central);

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

    updatePlaybackControls();
}

MainWindow::~MainWindow()
{
    if (m_processor->isRunning()) {
        m_processor->stopProcessing();
    }
}

void MainWindow::onFrameReady(const cv::Mat &frame)
{
    if (frame.empty()) return;

    QImage img;
    if (frame.channels() == 3) {
        cv::Mat rgb;
        cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);
        img = QImage(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888).copy();
    } else if (frame.channels() == 1) {
        img = QImage(frame.data, frame.cols, frame.rows, static_cast<int>(frame.step), QImage::Format_Grayscale8).copy();
    } else {
        return;
    }

    m_videoLabel->setPixmap(QPixmap::fromImage(img).scaled(
        m_videoLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void MainWindow::onStartFromFile()
{
    if (m_processor->isRunning()) {
        m_processor->stopProcessing();
    }
    QString file = QFileDialog::getOpenFileName(
        this, "Open video file", QDir::homePath(),
        "Video (*.mp4 *.avi *.mkv *.mov *.webm)");
    if (!file.isEmpty()) {
        m_processor->setSource(file, VideoProcessor::FromFile);
        m_isFileSource = true;
        m_btnPlayPause->setText("Pause");
        m_processor->start();
        updatePlaybackControls();
    }
}

void MainWindow::onStartFromCamera()
{
    if (m_processor->isRunning()) {
        m_processor->stopProcessing();
    }
    m_processor->setSource(QString(), VideoProcessor::FromCamera);
    m_isFileSource = false;
    m_processor->start();
    updatePlaybackControls();
}

void MainWindow::onStop()
{
    m_processor->requestStop();
}

void MainWindow::onPlayPause()
{
    if (!m_processor->isRunning() || !m_isFileSource) return;
    const bool nowPaused = !m_processor->isPaused();
    m_processor->setPaused(nowPaused);
    m_btnPlayPause->setText(nowPaused ? "Play" : "Pause");
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

void MainWindow::onProcessorFinished()
{
    m_isFileSource = false;
    m_btnPlayPause->setText("Pause");
    updatePlaybackControls();
}

void MainWindow::onError(const QString &message)
{
    QMessageBox::warning(this, tr("Video error"), message);
}

void MainWindow::updatePlaybackControls()
{
    const bool fileActive = m_isFileSource && m_processor->isRunning();
    m_btnPlayPause->setEnabled(fileActive);
    m_btnForward->setEnabled(fileActive);
    m_btnBackward->setEnabled(fileActive);
}