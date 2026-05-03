#ifndef DISC_TRACKER_KALMAN_TRACKER_H
#define DISC_TRACKER_KALMAN_TRACKER_H

#include <opencv2/video/tracking.hpp>
#include <opencv2/core.hpp>

namespace disc_tracker {

// Constant-velocity Kalman filter on (x, y) image coordinates.
// State: [x, y, vx, vy], measurement: [x, y].
class KalmanTracker
{
public:
    KalmanTracker();

    // Reset and seed with first observation.
    void init(const cv::Point2f& measurement);

    // Time update (advances state by one frame). Returns predicted center.
    cv::Point2f predict();

    // Measurement update with the observed center.
    cv::Point2f correct(const cv::Point2f& measurement);

    bool initialized() const { return m_initialized; }

private:
    cv::KalmanFilter m_kf;
    bool             m_initialized{false};
};

}  // namespace disc_tracker

#endif
