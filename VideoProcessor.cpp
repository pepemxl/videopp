#include "VideoProcessor.h"
#include "Filters.h"
#include <QFileInfo>
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

void VideoProcessor::seekToSeconds(double seconds)
{
    if (seconds < 0.0) seconds = 0.0;
    m_seekAbsSec.store(seconds);
    m_pauseCond.wakeAll();
}

void VideoProcessor::requestStartRecording(const QString &outputPath)
{
    {
        QMutexLocker lock(&m_recordPathMutex);
        m_pendingRecPath = outputPath;
    }
    m_recordStartRequested.store(true);
    m_pauseCond.wakeAll();
}

void VideoProcessor::requestStopRecording()
{
    m_recordStopRequested.store(true);
    m_pauseCond.wakeAll();
}

void VideoProcessor::run()
{
    m_stopped.store(false);
    m_paused.store(false);
    m_seekRequestSec.store(0.0);
    m_seekAbsSec.store(-1.0);
    m_recordStartRequested.store(false);
    m_recordStopRequested.store(false);
    m_recording.store(false);

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
    m_fps.store(fps);

    // Emit total duration for file sources (0 if unknown).
    double durationSec = 0.0;
    if (m_sourceType == FromFile) {
        const double total = cap.get(cv::CAP_PROP_FRAME_COUNT);
        if (total > 0.0 && fps > 0.0) durationSec = total / fps;
    }
    emit durationChanged(durationSec);

    // Honor a requested start position (file only).
    if (m_sourceType == FromFile) {
        double startSec = m_startSec.exchange(0.0);
        if (startSec > 0.0) {
            cap.set(cv::CAP_PROP_POS_MSEC, startSec * 1000.0);
        }
    }

    cv::VideoWriter writer;
    cv::Size writerSize;
    double recordWriteAccum = 0.0;  // fractional writes-per-frame accumulator

    while (!m_stopped.load()) {
        // Apply pending seek requests (file only — cameras can't seek).
        if (m_sourceType == FromFile) {
            double absSec = m_seekAbsSec.exchange(-1.0);
            if (absSec >= 0.0) {
                cap.set(cv::CAP_PROP_POS_MSEC, absSec * 1000.0);
            }
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

        // ----- Recording -----
        // Filter is applied on the recording copy so the saved file is
        // already filtered. The display path applies its own filter on the
        // raw frame in the GUI thread, which lets filter changes update the
        // currently-shown frame instantly.
        if (m_recordStartRequested.exchange(false) && !writer.isOpened()) {
            QString path;
            {
                QMutexLocker lock(&m_recordPathMutex);
                path = m_pendingRecPath;
            }
            const QString ext = QFileInfo(path).suffix().toLower();
            int fourcc = cv::VideoWriter::fourcc('M','J','P','G');
            if (ext == "mp4" || ext == "m4v" || ext == "mov" || ext == "mkv") {
                fourcc = cv::VideoWriter::fourcc('m','p','4','v');
            }
            writerSize = cv::Size(frame.cols, frame.rows);
            recordWriteAccum = 0.0;
            if (writer.open(path.toStdString(), fourcc, fps, writerSize, true)) {
                m_recording.store(true);
                emit recordingChanged(true);
            } else {
                emit error(QStringLiteral("Failed to open video writer for: %1").arg(path));
            }
        }
        if (m_recordStopRequested.exchange(false) && writer.isOpened()) {
            writer.release();
            m_recording.store(false);
            emit recordingChanged(false);
        }
        if (writer.isOpened()) {
            cv::Mat out = frame.clone();
            Filters::apply(m_filter.load(), out);
            if (out.channels() == 1) {
                cv::cvtColor(out, out, cv::COLOR_GRAY2BGR);
            }
            if (out.size() == writerSize) {
                // Speed-aware duplication: at 0.5x write each frame ~2x to preserve
                // slow motion; at 2x write every other frame to preserve speed-up.
                const double speed = m_speed.load();
                const double writesPerFrame = (speed > 0.0) ? (1.0 / speed) : 1.0;
                recordWriteAccum += writesPerFrame;
                int writes = static_cast<int>(recordWriteAccum);
                recordWriteAccum -= writes;
                for (int i = 0; i < writes; ++i) {
                    writer.write(out);
                }
            }
        }

        emit frameReady(frame);
        emit positionChanged(m_currentSec.load());

        if (m_sourceType == FromFile) {
            // If paused, block until resumed, stopped, or a seek is requested.
            // Pause is checked AFTER emitting so a seek-while-paused shows the new frame.
            if (m_paused.load()) {
                QMutexLocker lock(&m_pauseMutex);
                while (m_paused.load()
                       && !m_stopped.load()
                       && m_seekRequestSec.load() == 0.0
                       && m_seekAbsSec.load() < 0.0) {
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

    if (writer.isOpened()) {
        writer.release();
        m_recording.store(false);
        emit recordingChanged(false);
    }

    cap.release();
    m_fps.store(0.0);
    emit finished();
}
