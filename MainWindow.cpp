#include "MainWindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QWidget>
#include <QImage>
#include <QPixmap>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_processor(new VideoProcessor(this))
{
    m_videoLabel = new QLabel("Video display");
    m_videoLabel->setAlignment(Qt::AlignCenter);
    m_videoLabel->setMinimumSize(640, 480);

    m_btnStartFile = new QPushButton("Start from file");
    m_btnStartCam  = new QPushButton("Start from camera");
    m_btnStop      = new QPushButton("Stop");

    auto layout = new QVBoxLayout;
    layout->addWidget(m_videoLabel);
    layout->addWidget(m_btnStartFile);
    layout->addWidget(m_btnStartCam);
    layout->addWidget(m_btnStop);

    auto central = new QWidget;
    central->setLayout(layout);
    setCentralWidget(central);

    connect(m_btnStartFile, &QPushButton::clicked, this, &MainWindow::onStartFromFile);
    connect(m_btnStartCam,  &QPushButton::clicked, this, &MainWindow::onStartFromCamera);
    connect(m_btnStop,      &QPushButton::clicked, this, &MainWindow::onStop);

    connect(m_processor, &VideoProcessor::frameReady,
            this, &MainWindow::onFrameReady);
    connect(m_processor, &VideoProcessor::finished,
            this, &MainWindow::onStop);
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
    QString file = QFileDialog::getOpenFileName(
        this, "Open video file", QDir::homePath(),
        "Video (*.mp4 *.avi *.mkv)");
    if (!file.isEmpty()) {
        m_processor->setSource(file, VideoProcessor::FromFile);
        m_processor->start();
    }
}

void MainWindow::onStartFromCamera()
{
    m_processor->setSource(QString(), VideoProcessor::FromCamera);
    m_processor->start();
}

void MainWindow::onStop()
{
    m_processor->stopProcessing();
}