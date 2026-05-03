#ifndef PLAYER_HIGHLIGHTER_SEGMENTER_H
#define PLAYER_HIGHLIGHTER_SEGMENTER_H

#include "Config.h"
#include "PlayerCsvLoader.h"

#include <opencv2/core.hpp>

namespace player_highlighter {

// Player segmentation via GrabCut. Bbox sets the rect; visible skeleton
// joints become definite-foreground seeds (small disks) so the iteration
// converges on a tight player silhouette instead of leaking into background.
class Segmenter
{
public:
    explicit Segmenter(const Config& cfg);

    // Returns a CV_8UC1 mask the same size as `frame`. Non-zero == player.
    // If the frame's bbox is empty (player lost for that frame), returns
    // an all-zero mask.
    cv::Mat segment(const cv::Mat& frame, const PlayerFrame& pf);

private:
    int   m_iterations;
    int   m_margin;
    int   m_jointRadius;
    float m_visThreshold;
};

}  // namespace player_highlighter

#endif
