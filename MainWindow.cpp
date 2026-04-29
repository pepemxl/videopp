#include "MainWindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QWidget>
#include <QImage>

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
            this, [this](const cv::Mat &frame) {
                emit frameReady(frame);
            });
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
    // Procesa frame (ej: grayscaled, filtro, etc.)
    cv::Mat processed = matToGrayscale(frame);

    // Convierte a QImage para mostrarlo en QLabel
    QImage img(
        processed.data,
        processed.cols, processed.rows,
        static_cast<int>(processed.step),
        QImage::Format_Grayscale8
    );

    m_videoLabel->setPixmap(QPixmap::fromImage(img).scaled(
        m_videoLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

cv::Mat MainWindow::matToGrayscale(const cv::Mat &src)
{
    cv::Mat gray;
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    return gray;
}

void MainWindow::onBrowseFile()
{
    QString file = QFileDialog::getOpenFileName(
        this,
        "Open video file",
        QDir::homePath(),
        "Video files (*.mp4 *.avi *.mkv)");
    if (!file.isEmpty()) {
        // Guarda el path o pásalo directamente a VideoProcessor
    }
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