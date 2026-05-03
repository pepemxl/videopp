#ifndef DISC_TRACKER_DISC_TRACKER_H
#define DISC_TRACKER_DISC_TRACKER_H

#include "Config.h"
#include "Detection.h"
#include "HoughDetector.h"
#include "KalmanTracker.h"
#include "Markers.h"
#ifdef HAVE_OPENCV_DNN
#include "YoloDetector.h"
#endif

#include <memory>
#include <vector>

namespace disc_tracker {

struct PathPoint
{
    int    frameIdx;
    double timeMs;
    float  x;          // bbox top-left
    float  y;
    float  w;
    float  h;
    float  cx;         // smoothed center (Kalman output if enabled, else raw)
    float  cy;
    float  confidence;
    int    classId{-1};
    enum class Source { Detector, KalmanPredict, Anchor, Lost } source;
};

struct MarkerResidual
{
    Marker      marker;
    PathPoint   matched;       // tracker output at the closest frame
    double      timeDiffMs;    // |marker.time - matched.time|
    double      pixelError;    // Euclidean distance in pixels
    bool        hasMatch;      // false if tolerance window contained no match
};

struct RunReport
{
    std::vector<PathPoint>      path;
    std::vector<MarkerResidual> residuals;
    size_t detections{0};
    size_t kalmanBridged{0};
    size_t anchorOnly{0};
    size_t lost{0};
};

class DiscTracker
{
public:
    explicit DiscTracker(const Config& cfg);

    // Run end-to-end on the configured video. Returns the recovered path
    // along with marker residual statistics if a marker file was provided.
    RunReport run();

private:
    Config m_cfg;
    std::unique_ptr<HoughDetector> m_hough;
#ifdef HAVE_OPENCV_DNN
    std::unique_ptr<YoloDetector>  m_yolo;
#endif
    KalmanTracker m_kalman;

    std::vector<Detection> detect(const cv::Mat& frame);

    // Pick the detection that best matches a prior position. If lastValid is
    // null and gating is on, picks highest-confidence; otherwise picks the
    // detection nearest to lastValid that is within search_radius_px.
    int chooseBest(const std::vector<Detection>& dets,
                   const cv::Point2f* lastValid) const;

    // Same as chooseBest but with an explicit radius. When `strict` is true,
    // returns -1 if no candidate falls inside the gate (no fallback to
    // highest-confidence). Used by the marker-anchored path.
    int chooseBestGated(const std::vector<Detection>& dets,
                        const cv::Point2f* prior,
                        int radiusPx,
                        bool strict) const;
};

}  // namespace disc_tracker

#endif
