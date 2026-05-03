#ifndef PLAYER_TRACKER_CONFIG_H
#define PLAYER_TRACKER_CONFIG_H

#include <string>
#include <vector>

namespace player_tracker {

struct Config
{
    // --- video ---
    std::string videoPath;

    // --- markers (required: player marker provides the start + identity) ---
    std::string markersPath;

    // --- detector (YOLOv8/11-pose ONNX) ---
    std::string modelPath;
    int         inputSize{640};
    float       personConfThreshold{0.30f};
    float       nmsThreshold{0.45f};
    float       keypointVisThreshold{0.40f};   // per-keypoint min visibility

    // --- tracking window ---
    // Tracking runs from the first player marker to the first disc marker
    // (= release). The duration is sanity-checked / clamped to
    // [minDurationSec, maxDurationSec]; if the marker-derived window is
    // shorter or longer, it's padded backwards or trimmed at the end.
    double minDurationSec{4.0};
    double maxDurationSec{15.0};
    double prePadSec{0.0};   // extra pre-roll before first player marker

    // --- person identity ---
    int  personGateRadiusPx{200};   // soft gate around marker on first frame
    bool useIouTracking{true};      // pick the person with highest IoU vs prior

    // --- smoothing (per priority keypoint) ---
    bool useKeypointKalman{true};

    // --- output ---
    std::string csvPath;            // empty -> skip
    std::string overlayVideoPath;   // empty -> skip
    bool        drawAllKeypoints{true};
    bool        labelPriorityJoints{true};

    static Config loadFromFile(const std::string& path);
    void resolveRelativePaths(const std::string& configFilePath);

    // Inserts `_<timestamp>` (e.g. _20260503_152200) before the extension
    // of every output path that's set. Inputs are left untouched.
    void stampOutputPaths(const std::string& timestamp);
};

}  // namespace player_tracker

#endif
