#ifndef PLAYER_HIGHLIGHTER_HIGHLIGHTER_H
#define PLAYER_HIGHLIGHTER_HIGHLIGHTER_H

#include "Config.h"
#include "DepthEstimator.h"
#include "PlayerCsvLoader.h"
#include "Segmenter.h"
#include "ZoomTracker.h"

#include <memory>
#include <vector>

namespace player_highlighter {

struct RunReport
{
    size_t framesIn{0};        // CSV rows aligned to a video frame
    size_t framesWithPose{0};  // had a tracked detection
    size_t framesSegmented{0}; // mask was non-empty
    size_t framesDepth{0};     // depth map was computed
    size_t framesZoomed{0};    // wrote a zoom output frame
};

class Highlighter
{
public:
    explicit Highlighter(const Config& cfg);
    RunReport run();

private:
    Config m_cfg;
    Segmenter m_segmenter;
    std::unique_ptr<DepthEstimator> m_depth;

    cv::Mat composeOverlay(const cv::Mat& frame,
                           const PlayerFrame& pf,
                           const cv::Mat& mask,
                           const cv::Mat& depth8u);

    void drawSkeleton(cv::Mat& canvas, const PlayerFrame& pf) const;
};

}  // namespace player_highlighter

#endif
