#include "KalmanTracker.h"

namespace disc_tracker {

KalmanTracker::KalmanTracker()
    : m_kf(4, 2, 0, CV_32F)
{
    // State transition (constant velocity, dt=1 frame):
    //   x'  = x + vx
    //   y'  = y + vy
    //   vx' = vx
    //   vy' = vy
    m_kf.transitionMatrix = (cv::Mat_<float>(4, 4) <<
        1, 0, 1, 0,
        0, 1, 0, 1,
        0, 0, 1, 0,
        0, 0, 0, 1);

    // Measurement: pull (x, y) out of the state.
    cv::setIdentity(m_kf.measurementMatrix);

    // Tunable noise. Process noise > measurement noise here because real
    // disc motion isn't perfectly constant-velocity (spin, drag, bounces).
    cv::setIdentity(m_kf.processNoiseCov,     cv::Scalar::all(1e-2));
    cv::setIdentity(m_kf.measurementNoiseCov, cv::Scalar::all(1e-1));
    cv::setIdentity(m_kf.errorCovPost,        cv::Scalar::all(1.0));
}

void KalmanTracker::init(const cv::Point2f& measurement)
{
    m_kf.statePost.at<float>(0) = measurement.x;
    m_kf.statePost.at<float>(1) = measurement.y;
    m_kf.statePost.at<float>(2) = 0.f;
    m_kf.statePost.at<float>(3) = 0.f;
    cv::setIdentity(m_kf.errorCovPost, cv::Scalar::all(1.0));
    // Prime statePre / errorCovPre so a subsequent correct() — even
    // before any caller-side predict() — has well-defined inputs.
    m_kf.predict();
    m_initialized = true;
}

cv::Point2f KalmanTracker::predict()
{
    cv::Mat p = m_kf.predict();
    return { p.at<float>(0), p.at<float>(1) };
}

cv::Point2f KalmanTracker::correct(const cv::Point2f& measurement)
{
    cv::Mat z(2, 1, CV_32F);
    z.at<float>(0) = measurement.x;
    z.at<float>(1) = measurement.y;
    cv::Mat s = m_kf.correct(z);
    return { s.at<float>(0), s.at<float>(1) };
}

}  // namespace disc_tracker
