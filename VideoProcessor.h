#ifndef VIDEOPROCESSOR_H
#define VIDEOPROCESSOR_H

#include <opencv2/opencv.hpp>
#include <QThread>
#include <QMutex>

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
    ~VideoProcessor();

    void setSource(const QString &path, SourceType type);
    void stopProcessing();

signals:
    void frameReady(const cv::Mat &frame);  // frame procesado
    void finished();

protected:
    void run() override;

private:
    QString m_sourcePath;
    SourceType m_sourceType;
    cv::VideoCapture m_cap;
    QMutex m_mutex;
    bool m_stopped;
};

#endif