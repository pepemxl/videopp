#include "VideoProcessor.h"
#include <QMutexLocker>

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

void VideoProcessor::requestStop()
{
    m_stopped.store(true);
    m_pauseCond.wakeAll();
}

void VideoProcessor::stopProcessing()
{
    m_stopped.store(true);
    m_pauseCond.wakeAll();
    if (isRunning()) {
        wait();
    }
}

void VideoProcessor::setPaused(bool paused)
{
    m_paused.store(paused);
    if (!paused) {
        m_pauseCond.wakeAll();
    }
}

void VideoProcessor::seekRelativeSeconds(double seconds)
{
    // Accumulate so multiple rapid clicks before the worker consumes the request all apply.
    double prev = m_seekRequestSec.load();
    while (!m_seekRequestSec.compare_exchange_weak(prev, prev + seconds)) {}
    m_pauseCond.wakeAll();  // wake the worker if paused so the seek shows immediately
}

void VideoProcessor::setSpeed(double speed)
{
    if (speed < 0.1) speed = 0.1;
    if (speed > 8.0) speed = 8.0;
    m_speed.store(speed);
}

void VideoProcessor::run()
{
    m_stopped.store(false);
    m_paused.store(false);
    m_seekRequestSec.store(0.0);

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
    const double baseFrameDelayMs = 1000.0 / fps;

    // Honor a requested start position (file only).
    if (m_sourceType == FromFile) {
        double startSec = m_startSec.exchange(0.0);
        if (startSec > 0.0) {
            cap.set(cv::CAP_PROP_POS_MSEC, startSec * 1000.0);
        }
    }

    while (!m_stopped.load()) {
        // Apply pending seek (file only — cameras can't seek).
        if (m_sourceType == FromFile) {
            double seekSec = m_seekRequestSec.exchange(0.0);
            if (seekSec != 0.0) {
                double cur    = cap.get(cv::CAP_PROP_POS_FRAMES);
                double total  = cap.get(cv::CAP_PROP_FRAME_COUNT);
                double target = cur + seekSec * fps;
                if (target < 0.0) target = 0.0;
                if (total > 0.0 && target >= total) target = total - 1.0;
                cap.set(cv::CAP_PROP_POS_FRAMES, target);
            }
        }

        cv::Mat frame;
        if (!cap.read(frame) || frame.empty()) {
            break;
        }

        if (m_sourceType == FromFile) {
            const double posMs = cap.get(cv::CAP_PROP_POS_MSEC);
            if (posMs >= 0.0) m_currentSec.store(posMs / 1000.0);
        }

        if (m_grayscale.load()) {
            cv::cvtColor(frame, frame, cv::COLOR_BGR2GRAY);
        }
        if (m_blur.load()) {
            cv::GaussianBlur(frame, frame, cv::Size(15, 15), 0);
        }

        emit frameReady(frame);

        if (m_sourceType == FromFile) {
            // If paused, block until resumed, stopped, or a seek is requested.
            // Pause is checked AFTER emitting so a seek-while-paused shows the new frame.
            if (m_paused.load()) {
                QMutexLocker lock(&m_pauseMutex);
                while (m_paused.load()
                       && !m_stopped.load()
                       && m_seekRequestSec.load() == 0.0) {
                    m_pauseCond.wait(&m_pauseMutex);
                }
            } else {
                const double s = m_speed.load();
                int delay = static_cast<int>(baseFrameDelayMs / (s > 0.0 ? s : 1.0));
                if (delay < 1) delay = 1;
                QThread::msleep(delay);
            }
        }
    }

    cap.release();
    emit finished();
}
