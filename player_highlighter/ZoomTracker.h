#ifndef PLAYER_HIGHLIGHTER_ZOOM_TRACKER_H
#define PLAYER_HIGHLIGHTER_ZOOM_TRACKER_H

#include <opencv2/core.hpp>

namespace player_highlighter {

// EMA on bbox center + size to keep the zoom crop steady. Holds the last
// usable bbox if a frame has no pose, so the camera doesn't snap to (0,0).
class ZoomTracker
{
public:
    ZoomTracker(double alpha, double zoomFactor, cv::Size outputSize);

    // Update with the current-frame bbox. Empty bbox = "player lost this
    // frame" — the smoothed crop is held over from the previous frame.
    void update(const cv::Rect& bbox);

    // Returns the crop rect (clipped to `frameSize`). Empty rect == nothing
    // valid yet (no bbox has ever been seen).
    cv::Rect cropRect(cv::Size frameSize) const;

    cv::Size outputSize() const { return m_outputSize; }
    bool     primed()     const { return m_primed; }

private:
    double      m_alpha;
    double      m_zoomFactor;
    cv::Size    m_outputSize;
    bool        m_primed{false};
    cv::Point2f m_smoothedCenter{0.f, 0.f};
    cv::Size2f  m_smoothedSize{0.f, 0.f};
};

}  // namespace player_highlighter

#endif
