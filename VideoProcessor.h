#ifndef VIDEOPROCESSOR_H
#define VIDEOPROCESSOR_H

#include <opencv2/opencv.hpp>
#include <QThread>
#include <QString>
#include <QMutex>
#include <QWaitCondition>
#include <atomic>

class VideoProcessor : public QThread
{
    Q_OBJECT

public:
    enum SourceType
    {
        FromFile,
        FromCamera
    };

    explicit VideoProcessor(QObject *parent = nullptr);
    ~VideoProcessor() override;

    void setSource(const QString &path, SourceType type);
    void stopProcessing();
    void requestStop();

    // Playback controls — only meaningful for FromFile sources.
    void setPaused(bool paused);
    bool isPaused() const { return m_paused.load(); }
    void seekRelativeSeconds(double seconds);
    void seekToSeconds(double seconds);

    // Recording (writes filtered frames to disk while playback runs).
    void requestStartRecording(const QString &outputPath);
    void requestStopRecording();
    bool isRecording() const { return m_recording.load(); }

    // Playback speed (1.0 = normal). Clamped to [0.1, 8.0].
    void setSpeed(double speed);
    double speed() const { return m_speed.load(); }

    // Where to start the next run() (file sources only). 0 = beginning.
    void setStartPositionSec(double sec) { m_startSec.store(sec); }
    double currentPositionSec() const { return m_currentSec.load(); }

    // Filter toggles — applied per-frame inside run().
    void setGrayscaleEnabled(bool enabled) { m_grayscale.store(enabled); }
    void setBlurEnabled(bool enabled)      { m_blur.store(enabled); }
    bool isGrayscaleEnabled() const { return m_grayscale.load(); }
    bool isBlurEnabled() const      { return m_blur.load(); }

signals:
    void frameReady(const cv::Mat &frame);
    void error(const QString &message);
    void finished();
    void durationChanged(double seconds);   // 0 if unknown (e.g. camera/live)
    void positionChanged(double seconds);
    void recordingChanged(bool recording);

protected:
    void run() override;

private:
    QString             m_sourcePath;
    SourceType          m_sourceType{FromCamera};
    std::atomic<bool>   m_stopped{false};
    std::atomic<bool>   m_paused{false};
    std::atomic<bool>   m_grayscale{false};
    std::atomic<bool>   m_blur{false};
    std::atomic<double> m_seekRequestSec{0.0};
    std::atomic<double> m_seekAbsSec{-1.0};   // -1 = no absolute seek pending
    std::atomic<double> m_speed{1.0};
    std::atomic<double> m_startSec{0.0};
    std::atomic<double> m_currentSec{0.0};
    QMutex              m_pauseMutex;
    QWaitCondition      m_pauseCond;

    std::atomic<bool>   m_recordStartRequested{false};
    std::atomic<bool>   m_recordStopRequested{false};
    std::atomic<bool>   m_recording{false};
    QMutex              m_recordPathMutex;
    QString             m_pendingRecPath;
};

#endif
