#ifndef PLAYER_HIGHLIGHTER_DEPTH_ESTIMATOR_H
#define PLAYER_HIGHLIGHTER_DEPTH_ESTIMATOR_H

#include "Config.h"

#include <opencv2/core.hpp>
#include <opencv2/dnn.hpp>

namespace player_highlighter {

// Monocular depth estimation via a MiDaS ONNX export. Output is a relative
// depth map (NOT metric) at the input frame's resolution, normalized to
// [0, 255] CV_8U with cv::COLORMAP_* applied for visualization.
class DepthEstimator
{
public:
    explicit DepthEstimator(const Config& cfg);

    // Returns a CV_8UC1 normalized depth map at the size of `frame`,
    // ready to be passed to cv::applyColorMap.
    cv::Mat estimate(const cv::Mat& frame);

private:
    cv::dnn::Net m_net;
    int          m_inputSize;
    cv::Scalar   m_mean;
    cv::Scalar   m_std;
};

}  // namespace player_highlighter

#endif
