#include "KalmanTracker.h"

namespace player_tracker {

KalmanTracker::KalmanTracker()
    : m_kf(4, 2, 0, CV_32F)
{
    m_kf.transitionMatrix = (cv::Mat_<float>(4, 4) <<
        1, 0, 1, 0,
        0, 1, 0, 1,
        0, 0, 1, 0,
        0, 0, 0, 1);
    cv::setIdentity(m_kf.measurementMatrix);
    cv::setIdentity(m_kf.processNoiseCov,     cv::Scalar::all(1e-2));
    cv::setIdentity(m_kf.measurementNoiseCov, cv::Scalar::all(1e-1));
    cv::setIdentity(m_kf.errorCovPost,        cv::Scalar::all(1.0));
}

void KalmanTracker::init(const cv::Point2f& m)
{
    m_kf.statePost.at<float>(0) = m.x;
    m_kf.statePost.at<float>(1) = m.y;
    m_kf.statePost.at<float>(2) = 0.f;
    m_kf.statePost.at<float>(3) = 0.f;
    cv::setIdentity(m_kf.errorCovPost, cv::Scalar::all(1.0));
    // Prime statePre / errorCovPre so a subsequent correct() — even
    // before any caller-side predict() — has well-defined inputs.
    // Without this, correct() reads zero statePre and emits (0, 0).
    m_kf.predict();
    m_initialized = true;
}

cv::Point2f KalmanTracker::predict()
{
    cv::Mat p = m_kf.predict();
    return { p.at<float>(0), p.at<float>(1) };
}

cv::Point2f KalmanTracker::correct(const cv::Point2f& m)
{
    cv::Mat z(2, 1, CV_32F);
    z.at<float>(0) = m.x;
    z.at<float>(1) = m.y;
    cv::Mat s = m_kf.correct(z);
    return { s.at<float>(0), s.at<float>(1) };
}

}  // namespace player_tracker
