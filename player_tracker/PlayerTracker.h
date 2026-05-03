#ifndef PLAYER_TRACKER_PLAYER_TRACKER_H
#define PLAYER_TRACKER_PLAYER_TRACKER_H

#include "Config.h"
#include "KalmanTracker.h"
#include "Markers.h"
#include "PoseDetector.h"
#include "Skeleton.h"

#include <array>
#include <memory>
#include <vector>

namespace player_tracker {

struct FrameRecord
{
    int    frameIdx{-1};
    double timeMs{0.0};
    bool   hasPose{false};

    cv::Rect bbox{};
    float    personConf{0.f};

    // Smoothed (or raw) keypoints for this frame.
    std::array<Keypoint, kNumKeypoints> keypoints{};
};

struct RunReport
{
    std::vector<FrameRecord> frames;
    double startSec{0.0};
    double endSec{0.0};
    size_t framesWithPose{0};
    // Per-keypoint visibility counts across the run.
    std::array<size_t, kNumKeypoints> kpVisibleFrames{};
};

class PlayerTracker
{
public:
    explicit PlayerTracker(const Config& cfg);

    // Run the pipeline. Throws on missing model / unreadable video / no
    // valid time window.
    RunReport run();

private:
    Config m_cfg;
    std::unique_ptr<PoseDetector> m_detector;

    // Per-keypoint Kalman smoothers (only the priority ones are smoothed
    // by default; others are passed through unless useKeypointKalman is
    // set, in which case all 17 are smoothed).
    std::array<KalmanTracker, kNumKeypoints> m_kpKalman;

    // Compute the [start, end] time window from markers + duration policy.
    static void computeTimeWindow(const Marker& playerM,
                                  const Marker& discM,
                                  double prePadSec,
                                  double minDurationSec,
                                  double maxDurationSec,
                                  double videoDurationSec,
                                  double& startSec,
                                  double& endSec);

    // Pick the detection that best matches the player. On the first frame
    // gates by distance to the player marker; subsequently uses IoU vs.
    // the previous tracked bbox (or center distance as a fallback).
    int choosePerson(const std::vector<PoseDetection>& dets,
                     const cv::Point2f& priorPlayer,
                     const cv::Rect&    priorBbox,
                     bool               firstFrame) const;
};

}  // namespace player_tracker

#endif
