#include "DiscTracker.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace fs = std::filesystem;

namespace disc_tracker {

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

// Linear interpolation along the disc-marker timeline. Returns true and
// fills `out` if the time falls inside [first.time, last.time]; clamps to
// the endpoints when slightly outside; returns false if markers is empty.
bool interpolateMarker(const std::vector<Marker>& markers,
                       double timeSec, cv::Point2f& out)
{
    if (markers.empty()) return false;
    if (timeSec <= markers.front().timeSec) { out = markers.front().pos; return true; }
    if (timeSec >= markers.back().timeSec)  { out = markers.back().pos;  return true; }

    for (size_t i = 1; i < markers.size(); ++i) {
        if (timeSec <= markers[i].timeSec) {
            const Marker& a = markers[i - 1];
            const Marker& b = markers[i];
            const double dt = b.timeSec - a.timeSec;
            const float  t  = static_cast<float>(dt > 0 ? (timeSec - a.timeSec) / dt : 0.0);
            out = cv::Point2f(a.pos.x + (b.pos.x - a.pos.x) * t,
                              a.pos.y + (b.pos.y - a.pos.y) * t);
            return true;
        }
    }
    out = markers.back().pos;
    return true;
}

}  // namespace

DiscTracker::DiscTracker(const Config& cfg) : m_cfg(cfg)
{
    if (m_cfg.backend == Config::Backend::Yolo) {
#ifdef HAVE_OPENCV_DNN
        m_yolo = std::make_unique<YoloDetector>(m_cfg);
#else
        throw std::runtime_error(
            "Config selects backend=yolo but disc_tracker was built without "
            "OpenCV DNN support. Rebuild OpenCV with the 'dnn' feature "
            "(see disc_tracker/README.md) or switch backend to 'hough'.");
#endif
    } else {
        m_hough = std::make_unique<HoughDetector>(m_cfg);
    }
}

std::vector<Detection> DiscTracker::detect(const cv::Mat& frame)
{
#ifdef HAVE_OPENCV_DNN
    if (m_yolo) return m_yolo->detect(frame);
#endif
    return m_hough->detect(frame);
}

int DiscTracker::chooseBest(const std::vector<Detection>& dets,
                            const cv::Point2f* lastValid) const
{
    return chooseBestGated(dets, lastValid, m_cfg.searchRadiusPx,
                           /*strict=*/false);
}

int DiscTracker::chooseBestGated(const std::vector<Detection>& dets,
                                 const cv::Point2f* prior,
                                 int radiusPx,
                                 bool strict) const
{
    if (dets.empty()) return -1;

    if (prior && radiusPx > 0) {
        const float r2 = static_cast<float>(radiusPx) * static_cast<float>(radiusPx);
        int   best = -1;
        float bestDist2 = std::numeric_limits<float>::max();
        for (size_t i = 0; i < dets.size(); ++i) {
            const cv::Point2f c = dets[i].center();
            const float dx = c.x - prior->x;
            const float dy = c.y - prior->y;
            const float d2 = dx * dx + dy * dy;
            if (d2 <= r2 && d2 < bestDist2) {
                bestDist2 = d2;
                best = static_cast<int>(i);
            }
        }
        if (best >= 0) return best;
        if (strict)   return -1;   // gate was authoritative; no fallback
    }

    int   best = 0;
    float bestConf = dets[0].confidence;
    for (size_t i = 1; i < dets.size(); ++i) {
        if (dets[i].confidence > bestConf) {
            bestConf = dets[i].confidence;
            best = static_cast<int>(i);
        }
    }
    return best;
}

RunReport DiscTracker::run()
{
    cv::VideoCapture cap(m_cfg.videoPath);
    if (!cap.isOpened()) {
        throw std::runtime_error("Cannot open video: " + m_cfg.videoPath);
    }

    const double fps     = cap.get(cv::CAP_PROP_FPS);
    const int    frameW  = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
    const int    frameH  = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));

    // Optional: load markers, optionally seed Kalman + clip the time window.
    MarkerSet           markerSet;
    std::vector<Marker> discMarkers;
    bool                hasMarkers = false;
    if (!m_cfg.markersPath.empty()) {
        markerSet  = MarkerSet::loadFromFile(m_cfg.markersPath);
        discMarkers = markerSet.discMarkers();
        hasMarkers  = !discMarkers.empty();
    }

    double effectiveStartSec = m_cfg.startSec;
    double effectiveEndSec   = m_cfg.endSec;
    if (hasMarkers && m_cfg.clipToMarkerWindow) {
        const double pad = m_cfg.markerWindowPadSec;
        effectiveStartSec = std::max(0.0, discMarkers.front().timeSec - pad);
        effectiveEndSec   = discMarkers.back().timeSec + pad;
    }
    if (hasMarkers && m_cfg.seedFromMarkers && m_cfg.useKalman) {
        m_kalman.init(discMarkers.front().pos);
    }

    if (effectiveStartSec > 0.0) {
        cap.set(cv::CAP_PROP_POS_MSEC, effectiveStartSec * 1000.0);
    }

    cv::VideoWriter writer;
    if (!m_cfg.overlayVideoPath.empty()) {
        ensureParentDir(m_cfg.overlayVideoPath);
        const int fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
        writer.open(m_cfg.overlayVideoPath, fourcc,
                    fps > 1.0 ? fps : 30.0, cv::Size(frameW, frameH));
        if (!writer.isOpened()) {
            std::cerr << "[warn] could not open overlay writer: "
                      << m_cfg.overlayVideoPath << " (continuing without overlay)\n";
        }
    }

    std::ofstream csv;
    if (!m_cfg.csvPath.empty()) {
        ensureParentDir(m_cfg.csvPath);
        csv.open(m_cfg.csvPath);
        if (!csv) {
            throw std::runtime_error("Cannot open output CSV: " + m_cfg.csvPath);
        }
        csv << "frame_idx,time_ms,x,y,w,h,cx,cy,confidence,class_id,source\n";
    }

    std::vector<PathPoint>   path;
    std::vector<cv::Point2f> trail;          // smoothed centers for overlay
    bool        lastValid = false;
    cv::Point2f lastCenter;
    int         missed = 0;
    int         frameIdx = -1;

    cv::Mat frame;
    while (cap.read(frame)) {
        ++frameIdx;
        const double timeMs = cap.get(cv::CAP_PROP_POS_MSEC);

        if (effectiveEndSec > 0.0 && timeMs > effectiveEndSec * 1000.0) break;

        const std::vector<Detection> dets = detect(frame);

        // Compute the marker anchor for this frame, if anchoring is on.
        cv::Point2f anchor;
        bool        haveAnchor = false;
        if (hasMarkers && m_cfg.anchorToMarkers) {
            haveAnchor = interpolateMarker(discMarkers, timeMs / 1000.0, anchor);
        }

        // When anchored, gate strictly to the anchor with a tighter radius.
        // Otherwise fall back to the previous-frame prior with the looser
        // search_radius_px (or no gate at all).
        int best = -1;
        if (haveAnchor) {
            best = chooseBestGated(dets, &anchor,
                                   m_cfg.anchorRadiusPx, /*strict=*/true);
        } else {
            best = chooseBest(dets, lastValid ? &lastCenter : nullptr);
        }

        PathPoint pp{};
        pp.frameIdx = frameIdx;
        pp.timeMs   = timeMs;

        if (best >= 0) {
            const Detection& d = dets[best];
            const cv::Point2f c = d.center();

            cv::Point2f smoothed = c;
            if (m_cfg.useKalman) {
                if (!m_kalman.initialized()) m_kalman.init(c);
                else                         m_kalman.predict();
                smoothed = m_kalman.correct(c);
            }

            pp.x = static_cast<float>(d.bbox.x);
            pp.y = static_cast<float>(d.bbox.y);
            pp.w = static_cast<float>(d.bbox.width);
            pp.h = static_cast<float>(d.bbox.height);
            pp.cx = smoothed.x;
            pp.cy = smoothed.y;
            pp.confidence = d.confidence;
            pp.classId    = d.classId;
            pp.source = PathPoint::Source::Detector;

            lastValid  = true;
            lastCenter = smoothed;
            missed     = 0;

        } else if (haveAnchor && m_cfg.anchorFallback) {
            // No detection inside the marker-anchor gate — use the anchor
            // itself, optionally smoothed through the Kalman filter.
            cv::Point2f at = anchor;
            if (m_cfg.useKalman) {
                if (!m_kalman.initialized()) m_kalman.init(at);
                else                         m_kalman.predict();
                at = m_kalman.correct(at);
            }
            pp.cx = at.x;
            pp.cy = at.y;
            pp.x  = at.x;
            pp.y  = at.y;
            pp.w = pp.h = 0.f;
            pp.confidence = 0.f;
            pp.source = PathPoint::Source::Anchor;

            lastCenter = at;
            lastValid  = true;
            missed     = 0;

        } else if (m_cfg.useKalman && m_kalman.initialized() &&
                   missed < m_cfg.maxMissedFrames) {
            // Bridge a brief detection gap with the Kalman prediction.
            const cv::Point2f predicted = m_kalman.predict();
            pp.cx = predicted.x;
            pp.cy = predicted.y;
            pp.x  = predicted.x;
            pp.y  = predicted.y;
            pp.w = pp.h = 0.f;
            pp.confidence = 0.f;
            pp.source = PathPoint::Source::KalmanPredict;

            lastCenter = predicted;
            ++missed;

        } else {
            pp.source = PathPoint::Source::Lost;
            pp.confidence = 0.f;
            if (missed >= m_cfg.maxMissedFrames) lastValid = false;
            ++missed;
        }

        path.push_back(pp);

        if (csv) {
            const char* src = "lost";
            switch (pp.source) {
                case PathPoint::Source::Detector:      src = "detector";       break;
                case PathPoint::Source::KalmanPredict: src = "kalman_predict"; break;
                case PathPoint::Source::Anchor:        src = "anchor";         break;
                case PathPoint::Source::Lost:          src = "lost";           break;
            }
            csv << pp.frameIdx << ',' << pp.timeMs << ','
                << pp.x  << ',' << pp.y  << ',' << pp.w << ',' << pp.h << ','
                << pp.cx << ',' << pp.cy << ',' << pp.confidence << ','
                << pp.classId << ',' << src << '\n';
        }

        if (writer.isOpened()) {
            cv::Mat overlay = frame.clone();
            if (pp.source == PathPoint::Source::Detector) {
                trail.emplace_back(pp.cx, pp.cy);
                cv::rectangle(overlay,
                              cv::Rect(static_cast<int>(pp.x), static_cast<int>(pp.y),
                                       static_cast<int>(pp.w), static_cast<int>(pp.h)),
                              cv::Scalar(0, 255, 0), 2);
            } else if (pp.source == PathPoint::Source::KalmanPredict) {
                trail.emplace_back(pp.cx, pp.cy);
                cv::circle(overlay, cv::Point(static_cast<int>(pp.cx),
                                              static_cast<int>(pp.cy)),
                           6, cv::Scalar(0, 165, 255), 2);
            } else if (pp.source == PathPoint::Source::Anchor) {
                trail.emplace_back(pp.cx, pp.cy);
                cv::circle(overlay, cv::Point(static_cast<int>(pp.cx),
                                              static_cast<int>(pp.cy)),
                           5, cv::Scalar(255, 200, 0), 1);
            }
            if (m_cfg.drawTrajectory && trail.size() >= 2) {
                for (size_t i = 1; i < trail.size(); ++i) {
                    cv::line(overlay, trail[i - 1], trail[i],
                             cv::Scalar(0, 200, 255), 2);
                }
            }
            // Optional: draw markers as ground-truth crosshairs.
            if (hasMarkers) {
                for (const auto& m : discMarkers) {
                    if (std::abs(m.timeSec * 1000.0 - timeMs) < 250.0) {
                        cv::drawMarker(overlay,
                                       cv::Point(static_cast<int>(m.pos.x),
                                                 static_cast<int>(m.pos.y)),
                                       cv::Scalar(0, 0, 255),
                                       cv::MARKER_TILTED_CROSS, 18, 2);
                    }
                }
            }
            writer.write(overlay);
        }
    }

    RunReport report;
    report.path = std::move(path);

    // Residuals: for each disc marker, find the path point with the
    // closest timestamp (within tolerance) and report pixel error.
    if (hasMarkers && fps > 1.0) {
        const double frameMs = 1000.0 / fps;
        const double tolMs   = m_cfg.markerToleranceFrames * frameMs;
        for (const auto& m : discMarkers) {
            MarkerResidual r{};
            r.marker   = m;
            r.hasMatch = false;
            double bestDt = std::numeric_limits<double>::max();
            for (const auto& p : report.path) {
                // Skip anchor/lost rows: anchor mirrors the marker by
                // construction, so the residual would be ~0 and meaningless.
                if (p.source == PathPoint::Source::Lost ||
                    p.source == PathPoint::Source::Anchor) continue;
                const double dt = std::abs(p.timeMs - m.timeSec * 1000.0);
                if (dt < bestDt) {
                    bestDt = dt;
                    r.matched    = p;
                    r.timeDiffMs = dt;
                    const double dx = p.cx - m.pos.x;
                    const double dy = p.cy - m.pos.y;
                    r.pixelError = std::sqrt(dx * dx + dy * dy);
                    r.hasMatch   = (dt <= tolMs);
                }
            }
            report.residuals.push_back(r);
        }
    }

    for (const auto& p : report.path) {
        switch (p.source) {
            case PathPoint::Source::Detector:      ++report.detections;    break;
            case PathPoint::Source::KalmanPredict: ++report.kalmanBridged; break;
            case PathPoint::Source::Anchor:        ++report.anchorOnly;    break;
            case PathPoint::Source::Lost:          ++report.lost;          break;
        }
    }

    return report;
}

}  // namespace disc_tracker
