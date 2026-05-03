#include "ZoomTracker.h"

#include <algorithm>
#include <cmath>

namespace player_highlighter {

ZoomTracker::ZoomTracker(double alpha, double zoomFactor, cv::Size outputSize)
    : m_alpha(std::clamp(alpha, 0.01, 1.0)),
      m_zoomFactor(std::max(1.0, zoomFactor)),
      m_outputSize(outputSize)
{}

void ZoomTracker::update(const cv::Rect& bbox)
{
    if (bbox.area() <= 0) return;
    const cv::Point2f c(bbox.x + bbox.width  * 0.5f,
                        bbox.y + bbox.height * 0.5f);
    const cv::Size2f  s(static_cast<float>(bbox.width),
                        static_cast<float>(bbox.height));
    if (!m_primed) {
        m_smoothedCenter = c;
        m_smoothedSize   = s;
        m_primed         = true;
        return;
    }
    const float a = static_cast<float>(m_alpha);
    m_smoothedCenter = cv::Point2f(
        m_smoothedCenter.x * (1.f - a) + c.x * a,
        m_smoothedCenter.y * (1.f - a) + c.y * a);
    m_smoothedSize = cv::Size2f(
        m_smoothedSize.width  * (1.f - a) + s.width  * a,
        m_smoothedSize.height * (1.f - a) + s.height * a);
}

cv::Rect ZoomTracker::cropRect(cv::Size frameSize) const
{
    if (!m_primed) return {};

    // Crop must respect the zoom factor AND the output's aspect ratio so
    // resizing to outputSize is undistorted.
    const double bboxAspect = static_cast<double>(m_smoothedSize.width) /
                              std::max(1.0f, m_smoothedSize.height);
    const double outAspect  = static_cast<double>(m_outputSize.width) /
                              std::max(1, m_outputSize.height);

    double cropW = m_smoothedSize.width  * m_zoomFactor;
    double cropH = m_smoothedSize.height * m_zoomFactor;

    if (bboxAspect < outAspect) cropW = cropH * outAspect;
    else                        cropH = cropW / outAspect;

    int w = static_cast<int>(std::round(cropW));
    int h = static_cast<int>(std::round(cropH));
    int x = static_cast<int>(std::round(m_smoothedCenter.x - cropW * 0.5));
    int y = static_cast<int>(std::round(m_smoothedCenter.y - cropH * 0.5));

    // Clip while preserving size (shift instead of shrinking, where possible).
    if (x < 0)                         x = 0;
    if (y < 0)                         y = 0;
    if (x + w > frameSize.width)       x = frameSize.width  - w;
    if (y + h > frameSize.height)      y = frameSize.height - h;
    if (x < 0) { x = 0; w = std::min(w, frameSize.width); }
    if (y < 0) { y = 0; h = std::min(h, frameSize.height); }
    if (x + w > frameSize.width)  w = frameSize.width  - x;
    if (y + h > frameSize.height) h = frameSize.height - y;

    return { x, y, w, h };
}

}  // namespace player_highlighter
