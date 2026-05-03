#include "Highlighter.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace player_highlighter {

namespace {

void ensureParentDir(const std::string& path)
{
    if (path.empty()) return;
    fs::path p(path);
    if (p.has_parent_path()) {
        std::error_code ec;
        fs::create_directories(p.parent_path(), ec);
    }
}

cv::VideoWriter openWriter(const std::string& path, double fps, cv::Size size)
{
    if (path.empty()) return {};
    ensureParentDir(path);
    cv::VideoWriter w;
    w.open(path,
           cv::VideoWriter::fourcc('m', 'p', '4', 'v'),
           fps > 1.0 ? fps : 30.0, size);
    if (!w.isOpened()) {
        std::cerr << "[warn] cannot open video writer: " << path << "\n";
    }
    return w;
}

}  // namespace

Highlighter::Highlighter(const Config& cfg)
    : m_cfg(cfg), m_segmenter(cfg)
{
    if (m_cfg.enableDepth) {
        m_depth = std::make_unique<DepthEstimator>(m_cfg);
    }
}

void Highlighter::drawSkeleton(cv::Mat& canvas, const PlayerFrame& pf) const
{
    if (!pf.hasPose) return;
    for (const auto& e : kSkeletonEdges) {
        const auto& a = pf.keypoints[e.first];
        const auto& b = pf.keypoints[e.second];
        if (a.visibility < m_cfg.keypointVisThreshold ||
            b.visibility < m_cfg.keypointVisThreshold) continue;
        cv::line(canvas,
                 cv::Point(static_cast<int>(a.pos.x), static_cast<int>(a.pos.y)),
                 cv::Point(static_cast<int>(b.pos.x), static_cast<int>(b.pos.y)),
                 cv::Scalar(180, 180, 60), 2);
    }
    for (int kp = 0; kp < kNumKeypoints; ++kp) {
        const auto& k = pf.keypoints[kp];
        if (k.visibility < m_cfg.keypointVisThreshold) continue;
        cv::circle(canvas,
                   cv::Point(static_cast<int>(k.pos.x), static_cast<int>(k.pos.y)),
                   3, cv::Scalar(80, 220, 255), -1);
    }
}

cv::Mat Highlighter::composeOverlay(const cv::Mat& frame,
                                    const PlayerFrame& pf,
                                    const cv::Mat& mask,
                                    const cv::Mat& depth8u)
{
    cv::Mat out;
    const bool haveMask = !mask.empty() && cv::countNonZero(mask) > 0;

    switch (m_cfg.highlightMode) {
        case Config::HighlightMode::Outline: {
            out = frame.clone();
            if (haveMask) {
                std::vector<std::vector<cv::Point>> contours;
                cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
                cv::drawContours(out, contours, -1, cv::Scalar(0, 255, 255),
                                 m_cfg.contourThickness);
            }
            break;
        }

        case Config::HighlightMode::Dim: {
            cv::Mat dim;
            frame.convertTo(dim, -1, m_cfg.backgroundDim, 0);
            if (haveMask) {
                out = dim.clone();
                frame.copyTo(out, mask);
            } else {
                out = dim;
            }
            break;
        }

        case Config::HighlightMode::Silhouette: {
            out = frame.clone();
            if (haveMask) {
                cv::Mat colorLayer(frame.size(), frame.type(), m_cfg.silhouetteColor);
                cv::Mat blended;
                cv::addWeighted(out, 1.0 - m_cfg.silhouetteAlpha,
                                colorLayer, m_cfg.silhouetteAlpha, 0, blended);
                blended.copyTo(out, mask);
            }
            break;
        }

        case Config::HighlightMode::DepthOverlay: {
            out = frame.clone();
            if (!depth8u.empty()) {
                cv::Mat colored;
                cv::applyColorMap(depth8u, colored, m_cfg.depthColormap);
                cv::Mat blended;
                cv::addWeighted(out, 1.0 - m_cfg.depthAlpha,
                                colored, m_cfg.depthAlpha, 0, blended);
                if (haveMask) blended.copyTo(out, mask);
                else          out = blended;
            }
            break;
        }
    }

    if (m_cfg.drawBbox && pf.hasPose && pf.bbox.area() > 0) {
        cv::rectangle(out, pf.bbox, cv::Scalar(60, 220, 60), 2);
    }
    if (m_cfg.drawSkeleton) {
        drawSkeleton(out, pf);
    }

    return out;
}

RunReport Highlighter::run()
{
    auto rows = loadPlayerCsv(m_cfg.playerCsvPath);
    if (rows.empty()) {
        throw std::runtime_error("Player CSV has no data rows: " + m_cfg.playerCsvPath);
    }

    cv::VideoCapture cap(m_cfg.videoPath);
    if (!cap.isOpened()) {
        throw std::runtime_error("Cannot open video: " + m_cfg.videoPath);
    }

    const double fps   = cap.get(cv::CAP_PROP_FPS);
    const int    fW    = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
    const int    fH    = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));

    // The CSV reflects the player_tracker run window. Seek to its first
    // frame so the per-row alignment (CSV row N <-> the Nth video frame
    // we read in the tracking window) holds.
    if (rows.front().timeMs > 0.0) {
        cap.set(cv::CAP_PROP_POS_MSEC, rows.front().timeMs);
    }

    cv::VideoWriter overlayWriter = openWriter(m_cfg.overlayVideoPath, fps,
                                               cv::Size(fW, fH));
    cv::VideoWriter zoomWriter;
    if (m_cfg.enableZoom && !m_cfg.zoomVideoPath.empty()) {
        zoomWriter = openWriter(
            m_cfg.zoomVideoPath, fps,
            cv::Size(m_cfg.zoomOutputWidth, m_cfg.zoomOutputHeight));
    }

    ZoomTracker zoom(m_cfg.zoomSmoothingAlpha, m_cfg.zoomFactor,
                     cv::Size(m_cfg.zoomOutputWidth, m_cfg.zoomOutputHeight));

    RunReport report;

    cv::Mat frame;
    size_t rowIdx = 0;
    while (rowIdx < rows.size() && cap.read(frame)) {
        const PlayerFrame& pf = rows[rowIdx++];
        ++report.framesIn;
        if (pf.hasPose) ++report.framesWithPose;

        // Every mode benefits from the mask: outline draws the contour,
        // dim/silhouette use it as a stencil, depth_overlay limits the
        // depth tint to the player region.
        cv::Mat mask = m_segmenter.segment(frame, pf);
        if (cv::countNonZero(mask) > 0) ++report.framesSegmented;

        cv::Mat depth8u;
        if (m_cfg.enableDepth && m_depth) {
            depth8u = m_depth->estimate(frame);
            if (!depth8u.empty()) ++report.framesDepth;
        }

        cv::Mat overlay = composeOverlay(frame, pf, mask, depth8u);

        if (overlayWriter.isOpened()) overlayWriter.write(overlay);

        if (m_cfg.enableZoom && zoomWriter.isOpened()) {
            zoom.update(pf.bbox);
            const cv::Rect crop = zoom.cropRect(overlay.size());
            if (crop.area() > 0) {
                cv::Mat cropped = overlay(crop);
                cv::Mat resized;
                cv::resize(cropped, resized,
                           cv::Size(m_cfg.zoomOutputWidth, m_cfg.zoomOutputHeight));
                zoomWriter.write(resized);
                ++report.framesZoomed;
            }
        }
    }

    return report;
}

}  // namespace player_highlighter
