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

signals:
    void frameReady(const cv::Mat &frame);
    void error(const QString &message);
    void finished();

protected:
    void run() override;

private:
    QString             m_sourcePath;
    SourceType          m_sourceType{FromCamera};
    std::atomic<bool>   m_stopped{false};
    std::atomic<bool>   m_paused{false};
    std::atomic<double> m_seekRequestSec{0.0};
    QMutex              m_pauseMutex;
    QWaitCondition      m_pauseCond;
};

#endif
