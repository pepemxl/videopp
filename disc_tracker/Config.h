#ifndef DISC_TRACKER_CONFIG_H
#define DISC_TRACKER_CONFIG_H

#include <opencv2/core.hpp>
#include <string>
#include <vector>

namespace disc_tracker {

struct Config
{
    enum class Backend { Yolo, Hough };

    // --- video ---
    std::string videoPath;
    double      startSec{0.0};
    double      endSec{-1.0};      // -1 = run to end

    // --- detector ---
    Backend          backend{Backend::Yolo};
    std::string      modelPath;          // ONNX file (YOLOv8/11)
    int              inputSize{640};
    std::vector<int> classIds{29, 32};   // COCO: frisbee, sports ball
    float            confThreshold{0.30f};
    float            nmsThreshold{0.45f};

    // --- hough fallback ---
    double houghDp{1.2};
    double houghMinDist{30.0};
    double houghParam1{100.0};
    double houghParam2{30.0};
    int    houghMinRadius{5};
    int    houghMaxRadius{60};

    // --- tracker ---
    bool useKalman{true};
    int  maxMissedFrames{15};
    int  searchRadiusPx{0};   // 0 disables spatial gating

    // --- markers (optional ground-truth / Kalman seed / spatial anchor) ---
    std::string markersPath;
    bool        seedFromMarkers{true};   // init Kalman from first disc marker
    bool        clipToMarkerWindow{true};// auto-set start/end from marker times
    double      markerWindowPadSec{0.3}; // pad before/after marker window
    int         markerToleranceFrames{2};// residual matching window

    // --- marker-anchored gating (option 2 in the recommendation) ---
    // When enabled, each frame's expected disc position is computed by
    // linear interpolation between adjacent disc markers; the tracker only
    // accepts detections inside anchor_radius_px of that anchor and falls
    // back to the anchor itself when nothing is found in the gate.
    bool anchorToMarkers{false};
    int  anchorRadiusPx{60};
    bool anchorFallback{true};   // emit anchor as a path point when no detection

    // --- output ---
    std::string csvPath;
    std::string overlayVideoPath;        // empty -> skip
    bool        drawTrajectory{true};

    // Loads from YAML or JSON (extension drives the format).
    // Throws std::runtime_error on parse / missing required fields.
    static Config loadFromFile(const std::string& path);

    // Resolves relative paths inside the config against the config's directory.
    void resolveRelativePaths(const std::string& configFilePath);

    // Inserts `_<timestamp>` (e.g. _20260503_152200) before the extension
    // of every output path that's set. Inputs are left untouched.
    void stampOutputPaths(const std::string& timestamp);
};

}  // namespace disc_tracker

#endif
