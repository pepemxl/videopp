#ifndef VIDEOPROCESSOR_H
#define VIDEOPROCESSOR_H

#include <opencv2/opencv.hpp>
#include <QThread>
#include <QString>
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

signals:
    void frameReady(const cv::Mat &frame);
    void error(const QString &message);
    void finished();

protected:
    void run() override;

private:
    QString           m_sourcePath;
    SourceType        m_sourceType{FromCamera};
    std::atomic<bool> m_stopped{false};
};

#endif
