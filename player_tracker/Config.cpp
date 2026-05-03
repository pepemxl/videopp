#include "Config.h"

#include <opencv2/core/persistence.hpp>
#include <filesystem>
#include <stdexcept>

namespace fs = std::filesystem;

namespace player_tracker {

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

    readOpt(fs_["video_path"],   cfg.videoPath);
    readOpt(fs_["markers_path"], cfg.markersPath);

    readOpt(fs_["model_path"],            cfg.modelPath);
    readOpt(fs_["input_size"],            cfg.inputSize);
    readOpt(fs_["person_conf_threshold"], cfg.personConfThreshold);
    readOpt(fs_["nms_threshold"],         cfg.nmsThreshold);
    readOpt(fs_["keypoint_vis_threshold"],cfg.keypointVisThreshold);

    readOpt(fs_["min_duration_sec"], cfg.minDurationSec);
    readOpt(fs_["max_duration_sec"], cfg.maxDurationSec);
    readOpt(fs_["pre_pad_sec"],      cfg.prePadSec);

    readOpt(fs_["person_gate_radius_px"], cfg.personGateRadiusPx);
    readBool(fs_["use_iou_tracking"],     cfg.useIouTracking);
    readBool(fs_["use_keypoint_kalman"],  cfg.useKeypointKalman);

    readOpt(fs_["csv_path"],            cfg.csvPath);
    readOpt(fs_["overlay_video_path"],  cfg.overlayVideoPath);
    readBool(fs_["draw_all_keypoints"], cfg.drawAllKeypoints);
    readBool(fs_["label_priority_joints"], cfg.labelPriorityJoints);

    if (cfg.videoPath.empty())   throw std::runtime_error("Config: video_path is required");
    if (cfg.markersPath.empty()) throw std::runtime_error("Config: markers_path is required (player tracker needs the player marker)");
    if (cfg.modelPath.empty())   throw std::runtime_error("Config: model_path is required (YOLOv8/11-pose ONNX)");

    return cfg;
}

void Config::resolveRelativePaths(const std::string& configFilePath)
{
    fs::path base = fs::absolute(fs::path(configFilePath)).parent_path();
    resolveOne(videoPath,        base);
    resolveOne(markersPath,      base);
    resolveOne(modelPath,        base);
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

}  // namespace player_tracker
