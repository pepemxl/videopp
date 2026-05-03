#include "HoughDetector.h"

#include <opencv2/imgproc.hpp>
#include <cmath>

namespace disc_tracker {

HoughDetector::HoughDetector(const Config& cfg)
    : m_dp(cfg.houghDp), m_minDist(cfg.houghMinDist),
      m_param1(cfg.houghParam1), m_param2(cfg.houghParam2),
      m_minRadius(cfg.houghMinRadius), m_maxRadius(cfg.houghMaxRadius)
{}

std::vector<Detection> HoughDetector::detect(const cv::Mat& frame)
{
    cv::Mat gray;
    if (frame.channels() == 3) cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    else                       gray = frame;

    cv::medianBlur(gray, gray, 5);

    std::vector<cv::Vec3f> circles;
    cv::HoughCircles(gray, circles, cv::HOUGH_GRADIENT,
                     m_dp, m_minDist,
                     m_param1, m_param2,
                     m_minRadius, m_maxRadius);

    std::vector<Detection> out;
    out.reserve(circles.size());
    for (const auto& c : circles) {
        const float cx = c[0], cy = c[1], r = c[2];
        Detection d;
        d.bbox = cv::Rect(static_cast<int>(std::round(cx - r)),
                          static_cast<int>(std::round(cy - r)),
                          static_cast<int>(std::round(2.f * r)),
                          static_cast<int>(std::round(2.f * r)));
        d.confidence = 1.0f;   // Hough yields no probabilistic score
        d.classId    = -1;
        out.push_back(d);
    }
    return out;
}

}  // namespace disc_tracker
