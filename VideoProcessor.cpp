#include "VideoProcessor.h"

VideoProcessor::VideoProcessor(QObject *parent)
    : QThread(parent)
{
}

VideoProcessor::~VideoProcessor()
{
    stopProcessing();
}

void VideoProcessor::setSource(const QString &path, SourceType type)
{
    // Caller must stop the worker before changing the source.
    m_sourcePath = path;
    m_sourceType = type;
}

void VideoProcessor::stopProcessing()
{
    m_stopped.store(true);
    if (isRunning()) {
        wait();
    }
}

void VideoProcessor::run()
{
    m_stopped.store(false);

    cv::VideoCapture cap;
    bool opened = false;
    if (m_sourceType == FromCamera) {
#ifdef _WIN32
        // DirectShow tends to be the most reliable default on Windows.
        opened = cap.open(0, cv::CAP_DSHOW);
        if (!opened) {
            opened = cap.open(0, cv::CAP_ANY);
        }
#else
        opened = cap.open(0, cv::CAP_ANY);
#endif
    } else {
        opened = cap.open(m_sourcePath.toStdString());
    }

    if (!opened || !cap.isOpened()) {
        emit error(QStringLiteral("Failed to open video source"));
        emit finished();
        return;
    }

    double fps = cap.get(cv::CAP_PROP_FPS);
    if (fps <= 0.0 || fps > 240.0) fps = 30.0;
    const int frameDelayMs = static_cast<int>(1000.0 / fps);

    while (!m_stopped.load()) {
        cv::Mat frame;
        if (!cap.read(frame) || frame.empty()) {
            break;
        }

        cv::cvtColor(frame, frame, cv::COLOR_BGR2GRAY);
        cv::GaussianBlur(frame, frame, cv::Size(15, 15), 0);

        emit frameReady(frame);

        if (m_sourceType == FromFile) {
            QThread::msleep(frameDelayMs);
        }
    }

    cap.release();
    emit finished();
}
