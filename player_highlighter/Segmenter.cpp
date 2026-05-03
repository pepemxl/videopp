#include "Segmenter.h"

#include <opencv2/imgproc.hpp>

namespace player_highlighter {

Segmenter::Segmenter(const Config& cfg)
    : m_iterations(cfg.grabcutIterations),
      m_margin(cfg.grabcutMargin),
      m_jointRadius(cfg.jointSeedRadius),
      m_visThreshold(cfg.keypointVisThreshold)
{}

cv::Mat Segmenter::segment(const cv::Mat& frame, const PlayerFrame& pf)
{
    cv::Mat mask = cv::Mat::zeros(frame.size(), CV_8UC1);
    if (!pf.hasPose || pf.bbox.area() <= 0) return mask;

    // Run GrabCut on a padded crop of the bbox so we don't pay for the
    // full frame, but still include some background context.
    const cv::Rect frameRect(0, 0, frame.cols, frame.rows);
    cv::Rect crop(pf.bbox.x - m_margin, pf.bbox.y - m_margin,
                  pf.bbox.width  + 2 * m_margin,
                  pf.bbox.height + 2 * m_margin);
    crop &= frameRect;
    if (crop.area() <= 0) return mask;

    cv::Mat region = frame(crop).clone();
    if (region.channels() == 1) cv::cvtColor(region, region, cv::COLOR_GRAY2BGR);

    cv::Mat regionMask(region.size(), CV_8UC1, cv::Scalar(cv::GC_PR_BGD));

    // Inner rectangle (the bbox itself) starts as probable foreground.
    cv::Rect innerInRegion(pf.bbox.x - crop.x, pf.bbox.y - crop.y,
                           pf.bbox.width, pf.bbox.height);
    innerInRegion &= cv::Rect(0, 0, region.cols, region.rows);
    if (innerInRegion.area() > 0) {
        regionMask(innerInRegion).setTo(cv::GC_PR_FGD);
    }

    // Seed visible skeleton joints as definite foreground (small disks).
    for (int kp = 0; kp < kNumKeypoints; ++kp) {
        const auto& k = pf.keypoints[kp];
        if (k.visibility < m_visThreshold) continue;
        const cv::Point p(static_cast<int>(k.pos.x) - crop.x,
                          static_cast<int>(k.pos.y) - crop.y);
        if (p.x < 0 || p.y < 0 || p.x >= region.cols || p.y >= region.rows) continue;
        cv::circle(regionMask, p, m_jointRadius, cv::Scalar(cv::GC_FGD), -1);
    }

    cv::Mat bgdModel, fgdModel;
    try {
        cv::grabCut(region, regionMask, cv::Rect(),
                    bgdModel, fgdModel,
                    m_iterations, cv::GC_INIT_WITH_MASK);
    } catch (const cv::Exception&) {
        return mask;   // GrabCut occasionally fails on degenerate inputs
    }

    // Convert GrabCut's 4-value mask to a binary mask
    // (FG = GC_FGD | GC_PR_FGD).
    cv::Mat regionFg = (regionMask == cv::GC_FGD) | (regionMask == cv::GC_PR_FGD);
    regionFg.copyTo(mask(crop));
    return mask;
}

}  // namespace player_highlighter
