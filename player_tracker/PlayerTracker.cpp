#include "PlayerTracker.h"

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

namespace player_tracker {

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

float iou(const cv::Rect& a, const cv::Rect& b)
{
    const cv::Rect inter = a & b;
    const float interA = static_cast<float>(inter.area());
    if (interA <= 0.f) return 0.f;
    return interA / static_cast<float>(a.area() + b.area() - inter.area());
}

bool isPriority(int kp)
{
    for (int p : kPriorityKeypoints) if (p == kp) return true;
    return false;
}

}  // namespace

PlayerTracker::PlayerTracker(const Config& cfg) : m_cfg(cfg)
{
    m_detector = std::make_unique<PoseDetector>(m_cfg);
}

void PlayerTracker::computeTimeWindow(const Marker& playerM,
                                      const Marker& discM,
                                      double prePadSec,
                                      double minDurationSec,
                                      double maxDurationSec,
                                      double videoDurationSec,
                                      double& startSec,
                                      double& endSec)
{
    double s = std::max(0.0, playerM.timeSec - prePadSec);
    double e = discM.timeSec;   // first disc marker == release
    if (e < s) e = s;

    double dur = e - s;

    if (dur < minDurationSec) {
        // First try padding `s` backwards.
        const double targetS = std::max(0.0, e - minDurationSec);
        s = targetS;
        dur = e - s;
        // If we hit the start of the video, push `e` forward.
        if (dur < minDurationSec) {
            e = s + minDurationSec;
        }
    } else if (dur > maxDurationSec) {
        e = s + maxDurationSec;
    }

    if (videoDurationSec > 0.0) {
        if (e > videoDurationSec) e = videoDurationSec;
        if (s > e) s = e;
    }

    startSec = s;
    endSec   = e;
}

int PlayerTracker::choosePerson(const std::vector<PoseDetection>& dets,
                                const cv::Point2f& priorPlayer,
                                const cv::Rect&    priorBbox,
                                bool               firstFrame) const
{
    if (dets.empty()) return -1;

    if (firstFrame) {
        const float r2 = static_cast<float>(m_cfg.personGateRadiusPx) *
                         static_cast<float>(m_cfg.personGateRadiusPx);
        int   best = -1;
        float bestDist2 = std::numeric_limits<float>::max();
        for (size_t i = 0; i < dets.size(); ++i) {
            // Use the center of the bbox; keypoints aren't yet smoothed.
            const cv::Point2f c = dets[i].bboxCenter();
            const float dx = c.x - priorPlayer.x;
            const float dy = c.y - priorPlayer.y;
            const float d2 = dx * dx + dy * dy;
            if (d2 <= r2 && d2 < bestDist2) { bestDist2 = d2; best = static_cast<int>(i); }
        }
        if (best >= 0) return best;
        // No detection within the gate — fall back to highest-confidence.
        int   bk = 0;
        float bc = dets[0].personConf;
        for (size_t i = 1; i < dets.size(); ++i) {
            if (dets[i].personConf > bc) { bc = dets[i].personConf; bk = static_cast<int>(i); }
        }
        return bk;
    }

    if (m_cfg.useIouTracking && priorBbox.area() > 0) {
        int   best = -1;
        float bestIoU = 0.f;
        for (size_t i = 0; i < dets.size(); ++i) {
            const float v = iou(priorBbox, dets[i].bbox);
            if (v > bestIoU) { bestIoU = v; best = static_cast<int>(i); }
        }
        if (best >= 0 && bestIoU > 0.10f) return best;
    }

    // Center-distance fallback.
    const cv::Point2f priorCenter(
        priorBbox.x + priorBbox.width  * 0.5f,
        priorBbox.y + priorBbox.height * 0.5f);
    int   best = -1;
    float bestDist2 = std::numeric_limits<float>::max();
    for (size_t i = 0; i < dets.size(); ++i) {
        const cv::Point2f c = dets[i].bboxCenter();
        const float dx = c.x - priorCenter.x;
        const float dy = c.y - priorCenter.y;
        const float d2 = dx * dx + dy * dy;
        if (d2 < bestDist2) { bestDist2 = d2; best = static_cast<int>(i); }
    }
    return best;
}

RunReport PlayerTracker::run()
{
    auto markers = MarkerSet::loadFromFile(m_cfg.markersPath);
    const Marker playerM = markers.firstPlayerMarker();
    const Marker discM   = markers.firstDiscMarker();

    cv::VideoCapture cap(m_cfg.videoPath);
    if (!cap.isOpened()) {
        throw std::runtime_error("Cannot open video: " + m_cfg.videoPath);
    }

    const double fps = cap.get(cv::CAP_PROP_FPS);
    const int    fW  = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
    const int    fH  = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
    const double frameCount = cap.get(cv::CAP_PROP_FRAME_COUNT);
    const double videoDurationSec = (fps > 1.0 && frameCount > 0.0)
                                  ? frameCount / fps : 0.0;

    double startSec = 0.0, endSec = 0.0;
    computeTimeWindow(playerM, discM, m_cfg.prePadSec,
                      m_cfg.minDurationSec, m_cfg.maxDurationSec,
                      videoDurationSec, startSec, endSec);
    if (endSec <= startSec) {
        throw std::runtime_error("Computed time window is empty after clamping");
    }
    const double observedDur = endSec - startSec;
    if (observedDur + 1e-3 < m_cfg.minDurationSec) {
        std::cerr << "[warn] tracking window is " << observedDur
                  << "s, below min_duration_sec=" << m_cfg.minDurationSec
                  << " (likely truncated by video bounds)\n";
    }

    cap.set(cv::CAP_PROP_POS_MSEC, startSec * 1000.0);

    cv::VideoWriter writer;
    if (!m_cfg.overlayVideoPath.empty()) {
        ensureParentDir(m_cfg.overlayVideoPath);
        const int fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
        writer.open(m_cfg.overlayVideoPath, fourcc,
                    fps > 1.0 ? fps : 30.0, cv::Size(fW, fH));
        if (!writer.isOpened()) {
            std::cerr << "[warn] cannot open overlay writer: "
                      << m_cfg.overlayVideoPath << "\n";
        }
    }

    std::ofstream csv;
    if (!m_cfg.csvPath.empty()) {
        ensureParentDir(m_cfg.csvPath);
        csv.open(m_cfg.csvPath);
        if (!csv) throw std::runtime_error("Cannot open CSV: " + m_cfg.csvPath);
        csv << "frame_idx,time_ms,has_pose,bbox_x,bbox_y,bbox_w,bbox_h,person_conf";
        for (int kp = 0; kp < kNumKeypoints; ++kp) {
            csv << ',' << kKeypointNames[kp] << "_x"
                << ',' << kKeypointNames[kp] << "_y"
                << ',' << kKeypointNames[kp] << "_v";
        }
        csv << '\n';
    }

    RunReport report;
    report.startSec = startSec;
    report.endSec   = endSec;

    cv::Mat frame;
    int   frameIdx = -1;
    bool  haveTracked = false;
    cv::Rect lastBbox{};
    for (auto& kf : m_kpKalman) kf = KalmanTracker{};

    while (cap.read(frame)) {
        ++frameIdx;
        const double timeMs = cap.get(cv::CAP_PROP_POS_MSEC);
        if (timeMs > endSec * 1000.0) break;

        FrameRecord rec{};
        rec.frameIdx = frameIdx;
        rec.timeMs   = timeMs;

        const std::vector<PoseDetection> dets = m_detector->detect(frame);
        const int pick = choosePerson(dets,
                                      playerM.pos,
                                      lastBbox,
                                      /*firstFrame=*/!haveTracked);

        if (pick >= 0) {
            const PoseDetection& d = dets[pick];
            rec.hasPose    = true;
            rec.bbox       = d.bbox;
            rec.personConf = d.personConf;
            haveTracked    = true;
            lastBbox       = d.bbox;
            ++report.framesWithPose;

            for (int kp = 0; kp < kNumKeypoints; ++kp) {
                const Keypoint& raw = d.keypoints[kp];
                Keypoint smoothed = raw;
                const bool visible = raw.visibility >= m_cfg.keypointVisThreshold;

                if (m_cfg.useKeypointKalman && visible) {
                    auto& kf = m_kpKalman[kp];
                    if (!kf.initialized()) kf.init(raw.pos);
                    else                   kf.predict();
                    smoothed.pos = kf.correct(raw.pos);
                }
                rec.keypoints[kp] = smoothed;
                if (visible) ++report.kpVisibleFrames[kp];
            }
        }

        // CSV
        if (csv) {
            csv << rec.frameIdx << ',' << rec.timeMs << ','
                << (rec.hasPose ? 1 : 0) << ','
                << rec.bbox.x << ',' << rec.bbox.y << ','
                << rec.bbox.width << ',' << rec.bbox.height << ','
                << rec.personConf;
            for (int kp = 0; kp < kNumKeypoints; ++kp) {
                const auto& k = rec.keypoints[kp];
                csv << ',' << k.pos.x << ',' << k.pos.y << ',' << k.visibility;
            }
            csv << '\n';
        }

        // Overlay rendering
        if (writer.isOpened()) {
            cv::Mat overlay = frame.clone();
            if (rec.hasPose) {
                cv::rectangle(overlay, rec.bbox, cv::Scalar(60, 220, 60), 2);

                // Skeleton edges
                for (const auto& e : kSkeletonEdges) {
                    const auto& a = rec.keypoints[e.first];
                    const auto& b = rec.keypoints[e.second];
                    if (a.visibility < m_cfg.keypointVisThreshold ||
                        b.visibility < m_cfg.keypointVisThreshold) continue;
                    cv::line(overlay,
                             cv::Point(static_cast<int>(a.pos.x),
                                       static_cast<int>(a.pos.y)),
                             cv::Point(static_cast<int>(b.pos.x),
                                       static_cast<int>(b.pos.y)),
                             cv::Scalar(180, 180, 60), 2);
                }

                // Keypoints
                for (int kp = 0; kp < kNumKeypoints; ++kp) {
                    const auto& k = rec.keypoints[kp];
                    if (k.visibility < m_cfg.keypointVisThreshold) continue;
                    const cv::Point p(static_cast<int>(k.pos.x),
                                      static_cast<int>(k.pos.y));
                    if (isPriority(kp)) {
                        cv::circle(overlay, p, 6, cv::Scalar(0, 0, 255), -1);
                        cv::circle(overlay, p, 8, cv::Scalar(255, 255, 255), 1);
                        if (m_cfg.labelPriorityJoints) {
                            cv::putText(overlay,
                                        std::string(kKeypointNames[kp]),
                                        p + cv::Point(8, -6),
                                        cv::FONT_HERSHEY_SIMPLEX, 0.4,
                                        cv::Scalar(255, 255, 255), 1);
                        }
                    } else if (m_cfg.drawAllKeypoints) {
                        cv::circle(overlay, p, 3, cv::Scalar(80, 200, 255), -1);
                    }
                }
            }
            writer.write(overlay);
        }

        report.frames.push_back(rec);
    }

    return report;
}

}  // namespace player_tracker
