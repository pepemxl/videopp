#ifndef PLAYER_TRACKER_KALMAN_TRACKER_H
#define PLAYER_TRACKER_KALMAN_TRACKER_H

#include <opencv2/video/tracking.hpp>
#include <opencv2/core.hpp>

namespace player_tracker {

// Constant-velocity Kalman filter on (x, y). State [x, y, vx, vy].
// Used per-keypoint to smooth the pose estimate frame-to-frame.
class KalmanTracker
{
public:
    KalmanTracker();

    void        init(const cv::Point2f& measurement);
    cv::Point2f predict();
    cv::Point2f correct(const cv::Point2f& measurement);
    bool        initialized() const { return m_initialized; }

private:
    cv::KalmanFilter m_kf;
    bool             m_initialized{false};
};

}  // namespace player_tracker

#endif
