#ifndef PLAYER_TRACKER_POSE_DETECTOR_H
#define PLAYER_TRACKER_POSE_DETECTOR_H

#include "Skeleton.h"
#include "Config.h"

#include <opencv2/dnn.hpp>
#include <array>
#include <vector>

namespace player_tracker {

struct Keypoint
{
    cv::Point2f pos{0.f, 0.f};
    float       visibility{0.f};   // 0..1; below threshold == not seen this frame
};

struct PoseDetection
{
    cv::Rect                              bbox;
    float                                 personConf{0.f};
    std::array<Keypoint, kNumKeypoints>   keypoints{};

    cv::Point2f bboxCenter() const
    {
        return { bbox.x + bbox.width  * 0.5f,
                 bbox.y + bbox.height * 0.5f };
    }
};

// YOLOv8/11-pose ONNX wrapped through OpenCV DNN.
// Expected ONNX output layout:  [1, 4 + 1 + numKpt*3, numAnchors]
//   - 4 box coords (cx, cy, w, h) in input-letterbox pixels
//   - 1 class score (person)
//   - numKpt * (x, y, visibility), kpt coords in input-letterbox pixels
class PoseDetector
{
public:
    explicit PoseDetector(const Config& cfg);

    std::vector<PoseDetection> detect(const cv::Mat& frame);

private:
    cv::dnn::Net m_net;
    int          m_inputSize;
    float        m_personConfThreshold;
    float        m_nmsThreshold;
};

}  // namespace player_tracker

#endif
