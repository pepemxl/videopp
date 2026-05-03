#ifndef DISC_TRACKER_YOLO_DETECTOR_H
#define DISC_TRACKER_YOLO_DETECTOR_H

#include "Detection.h"
#include "Config.h"

#include <opencv2/dnn.hpp>
#include <vector>

namespace disc_tracker {

// YOLOv8 / YOLO11 ONNX detector run through OpenCV DNN.
// Expected output layout: [1, 4 + numClasses, numAnchors] (Ultralytics export).
class YoloDetector
{
public:
    explicit YoloDetector(const Config& cfg);
    std::vector<Detection> detect(const cv::Mat& frame);

private:
    cv::dnn::Net     m_net;
    int              m_inputSize;
    std::vector<int> m_classIds;
    float            m_confThreshold;
    float            m_nmsThreshold;
};

}  // namespace disc_tracker

#endif
