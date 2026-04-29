#include "VideoProcessor.h"
#include <QMutexLocker>

VideoProcessor::VideoProcessor(QObject *parent)
    : QThread(parent), m_stopped(false)
{
}

VideoProcessor::~VideoProcessor()
{
    stopProcessing();
}

void VideoProcessor::setSource(const QString &path, SourceType type)
{
    QMutexLocker locker(&m_mutex);

    m_sourcePath = path;
    m_sourceType  = type;

    if (m_stopped) {
        return;
    }

    if (m_sourceType == FromCamera) {
        m_cap.open(0);  // cámara default
    } else {
        m_cap.open(m_sourcePath.toStdString());
    }

    if (!m_cap.isOpened()) {
        // emit error via Qt signal si quieres
    }
}

void VideoProcessor::stopProcessing()
{
    m_mutex.lock();
    m_stopped = true;
    m_mutex.unlock();

    if (isRunning()) {
        wait();
    }
}

void VideoProcessor::run()
{
    m_stopped = false;

    while (!m_stopped) {
        cv::Mat frame;
        {
            QMutexLocker locker(&m_mutex);
            if (!m_cap.read(frame)) {
                break;  // fin de video o error
            }
        }

        if (frame.empty()) {
            continue;
        }

        // Procesa el frame aquí (por ejemplo OpenCV transformations)
        // cv::cvtColor(frame, frame, cv::COLOR_BGR2GRAY);
        // cv::GaussianBlur(frame, frame, cv::Size(15, 15), 0);

        emit frameReady(frame);
    }

    emit finished();
}