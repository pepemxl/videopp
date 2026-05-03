#include "Config.h"

#include <opencv2/core/persistence.hpp>
#include <filesystem>
#include <stdexcept>

namespace fs = std::filesystem;

namespace disc_tracker {

namespace {

template <typename T>
void readOpt(const cv::FileNode& node, T& dst)
{
    if (!node.empty() && !node.isNone()) node >> dst;
}

void readBool(const cv::FileNode& node, bool& dst)
{
    if (node.empty() || node.isNone()) return;
    int v = dst ? 1 : 0;
    node >> v;
    dst = (v != 0);
}

std::string toLower(std::string s)
{
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

void resolveOne(std::string& p, const fs::path& base)
{
    if (p.empty()) return;
    fs::path candidate(p);
    if (candidate.is_absolute()) return;
    p = (base / candidate).lexically_normal().string();
}

}  // namespace

Config Config::loadFromFile(const std::string& path)
{
    cv::FileStorage fs_(path, cv::FileStorage::READ);
    if (!fs_.isOpened()) {
        throw std::runtime_error("Cannot open config file: " + path);
    }

    Config cfg;

    readOpt(fs_["video_path"],  cfg.videoPath);
    readOpt(fs_["start_sec"],   cfg.startSec);
    readOpt(fs_["end_sec"],     cfg.endSec);

    if (cfg.videoPath.empty()) {
        throw std::runtime_error("Config missing required field: video_path");
    }

    std::string backend = "yolo";
    readOpt(fs_["backend"], backend);
    backend = toLower(backend);
    if (backend == "yolo")       cfg.backend = Backend::Yolo;
    else if (backend == "hough") cfg.backend = Backend::Hough;
    else throw std::runtime_error("Unknown backend (expected yolo|hough): " + backend);

    readOpt(fs_["model_path"],     cfg.modelPath);
    readOpt(fs_["input_size"],     cfg.inputSize);
    readOpt(fs_["class_ids"],      cfg.classIds);
    readOpt(fs_["conf_threshold"], cfg.confThreshold);
    readOpt(fs_["nms_threshold"],  cfg.nmsThreshold);

    readOpt(fs_["hough_dp"],         cfg.houghDp);
    readOpt(fs_["hough_min_dist"],   cfg.houghMinDist);
    readOpt(fs_["hough_param1"],     cfg.houghParam1);
    readOpt(fs_["hough_param2"],     cfg.houghParam2);
    readOpt(fs_["hough_min_radius"], cfg.houghMinRadius);
    readOpt(fs_["hough_max_radius"], cfg.houghMaxRadius);

    readBool(fs_["use_kalman"],       cfg.useKalman);
    readOpt(fs_["max_missed_frames"], cfg.maxMissedFrames);
    readOpt(fs_["search_radius_px"],  cfg.searchRadiusPx);

    readOpt(fs_["markers_path"],            cfg.markersPath);
    readBool(fs_["seed_from_markers"],      cfg.seedFromMarkers);
    readBool(fs_["clip_to_marker_window"],  cfg.clipToMarkerWindow);
    readOpt(fs_["marker_window_pad_sec"],   cfg.markerWindowPadSec);
    readOpt(fs_["marker_tolerance_frames"], cfg.markerToleranceFrames);

    readBool(fs_["anchor_to_markers"], cfg.anchorToMarkers);
    readOpt(fs_["anchor_radius_px"],   cfg.anchorRadiusPx);
    readBool(fs_["anchor_fallback"],   cfg.anchorFallback);

    readOpt(fs_["csv_path"],            cfg.csvPath);
    readOpt(fs_["overlay_video_path"],  cfg.overlayVideoPath);
    readBool(fs_["draw_trajectory"],    cfg.drawTrajectory);

    if (cfg.backend == Backend::Yolo && cfg.modelPath.empty()) {
        throw std::runtime_error(
            "Config: backend=yolo requires model_path (path to YOLOv8/11 ONNX)");
    }

    return cfg;
}

void Config::resolveRelativePaths(const std::string& configFilePath)
{
    fs::path base = fs::absolute(fs::path(configFilePath)).parent_path();
    resolveOne(videoPath,        base);
    resolveOne(modelPath,        base);
    resolveOne(markersPath,      base);
    resolveOne(csvPath,          base);
    resolveOne(overlayVideoPath, base);
}

namespace {
void stampOne(std::string& p, const std::string& timestamp)
{
    if (p.empty()) return;
    fs::path fp(p);
    fs::path parent = fp.parent_path();
    std::string stem = fp.stem().string();
    std::string ext  = fp.extension().string();
    fp = parent / (stem + "_" + timestamp + ext);
    p = fp.string();
}
}  // namespace

void Config::stampOutputPaths(const std::string& timestamp)
{
    stampOne(csvPath,          timestamp);
    stampOne(overlayVideoPath, timestamp);
}

}  // namespace disc_tracker
