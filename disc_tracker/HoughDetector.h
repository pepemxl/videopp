#ifndef DISC_TRACKER_HOUGH_DETECTOR_H
#define DISC_TRACKER_HOUGH_DETECTOR_H

#include "Detection.h"
#include "Config.h"

#include <vector>

namespace disc_tracker {

// Classical Hough-circle fallback. Not ML — only here so the binary
// can run end-to-end on builds where opencv_dnn isn't available.
class HoughDetector
{
public:
    explicit HoughDetector(const Config& cfg);
    std::vector<Detection> detect(const cv::Mat& frame);

private:
    double m_dp;
    double m_minDist;
    double m_param1;
    double m_param2;
    int    m_minRadius;
    int    m_maxRadius;
};

}  // namespace disc_tracker

#endif
