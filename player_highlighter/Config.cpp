#include "Config.h"

#include <opencv2/core/persistence.hpp>
#include <filesystem>
#include <stdexcept>
#include <vector>

namespace fs = std::filesystem;

namespace player_highlighter {

namespace {

template <typename T>
void readOpt(const cv::FileNode& n, T& dst)
{
    if (!n.empty() && !n.isNone()) n >> dst;
}

void readBool(const cv::FileNode& n, bool& dst)
{
    if (n.empty() || n.isNone()) return;
    int v = dst ? 1 : 0;
    n >> v;
    dst = (v != 0);
}

void readScalar(const cv::FileNode& n, cv::Scalar& dst)
{
    if (n.empty() || n.isNone() || !n.isSeq()) return;
    std::vector<double> vals;
    n >> vals;
    for (size_t i = 0; i < vals.size() && i < 4; ++i) dst[static_cast<int>(i)] = vals[i];
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

    readOpt(fs_["video_path"],      cfg.videoPath);
    readOpt(fs_["player_csv_path"], cfg.playerCsvPath);

    std::string mode = "dim";
    readOpt(fs_["highlight_mode"], mode);
    mode = toLower(mode);
    if      (mode == "outline")       cfg.highlightMode = HighlightMode::Outline;
    else if (mode == "dim")           cfg.highlightMode = HighlightMode::Dim;
    else if (mode == "silhouette")    cfg.highlightMode = HighlightMode::Silhouette;
    else if (mode == "depth_overlay") cfg.highlightMode = HighlightMode::DepthOverlay;
    else throw std::runtime_error(
        "Unknown highlight_mode (expected outline|dim|silhouette|depth_overlay): " + mode);

    readScalar(fs_["silhouette_color"], cfg.silhouetteColor);
    readOpt(fs_["silhouette_alpha"],    cfg.silhouetteAlpha);
    readOpt(fs_["background_dim"],      cfg.backgroundDim);
    readBool(fs_["draw_skeleton"],      cfg.drawSkeleton);
    readBool(fs_["draw_bbox"],          cfg.drawBbox);
    readOpt(fs_["contour_thickness"],   cfg.contourThickness);

    readOpt(fs_["grabcut_iterations"],       cfg.grabcutIterations);
    readOpt(fs_["grabcut_margin"],           cfg.grabcutMargin);
    readOpt(fs_["joint_seed_radius"],        cfg.jointSeedRadius);
    readOpt(fs_["keypoint_vis_threshold"],   cfg.keypointVisThreshold);

    readBool(fs_["enable_depth"],   cfg.enableDepth);
    readOpt(fs_["depth_model_path"], cfg.depthModelPath);
    readOpt(fs_["depth_input_size"], cfg.depthInputSize);
    readScalar(fs_["depth_mean"],    cfg.depthMean);
    readScalar(fs_["depth_std"],     cfg.depthStd);
    readOpt(fs_["depth_colormap"],   cfg.depthColormap);
    readOpt(fs_["depth_alpha"],      cfg.depthAlpha);

    readBool(fs_["enable_zoom"],         cfg.enableZoom);
    readOpt(fs_["zoom_factor"],          cfg.zoomFactor);
    readOpt(fs_["zoom_output_width"],    cfg.zoomOutputWidth);
    readOpt(fs_["zoom_output_height"],   cfg.zoomOutputHeight);
    readOpt(fs_["zoom_smoothing_alpha"], cfg.zoomSmoothingAlpha);

    readOpt(fs_["overlay_video_path"], cfg.overlayVideoPath);
    readOpt(fs_["zoom_video_path"],    cfg.zoomVideoPath);

    if (cfg.videoPath.empty())      throw std::runtime_error("Config: video_path is required");
    if (cfg.playerCsvPath.empty())  throw std::runtime_error("Config: player_csv_path is required");

    if (cfg.highlightMode == HighlightMode::DepthOverlay && !cfg.enableDepth) {
        cfg.enableDepth = true;   // DepthOverlay implies depth on
    }
    if (cfg.enableDepth && cfg.depthModelPath.empty()) {
        throw std::runtime_error(
            "Config: enable_depth=1 requires depth_model_path (MiDaS ONNX)");
    }

    return cfg;
}

void Config::resolveRelativePaths(const std::string& configFilePath)
{
    fs::path base = fs::absolute(fs::path(configFilePath)).parent_path();
    resolveOne(videoPath,        base);
    resolveOne(playerCsvPath,    base);
    resolveOne(depthModelPath,   base);
    resolveOne(overlayVideoPath, base);
    resolveOne(zoomVideoPath,    base);
}

}  // namespace player_highlighter
