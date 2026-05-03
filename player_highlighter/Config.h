#ifndef PLAYER_HIGHLIGHTER_CONFIG_H
#define PLAYER_HIGHLIGHTER_CONFIG_H

#include <opencv2/core.hpp>
#include <string>

namespace player_highlighter {

struct Config
{
    enum class HighlightMode { Outline, Dim, Silhouette, DepthOverlay };

    // --- inputs ---
    std::string videoPath;
    std::string playerCsvPath;     // produced by player_tracker

    // --- highlight mode ---
    HighlightMode highlightMode{HighlightMode::Dim};
    cv::Scalar    silhouetteColor{0, 200, 255};   // BGR — used when mode = silhouette
    double        silhouetteAlpha{0.45};          // blend factor 0..1
    double        backgroundDim{0.35};            // 0 = black bg, 1 = no dim
    bool          drawSkeleton{true};
    bool          drawBbox{true};
    int           contourThickness{2};

    // --- segmentation (GrabCut) ---
    int   grabcutIterations{3};
    int   grabcutMargin{20};        // px around bbox before running GrabCut
    int   jointSeedRadius{4};       // definite-foreground dilation around joints
    float keypointVisThreshold{0.40f};

    // --- depth (optional, MiDaS ONNX) ---
    bool        enableDepth{false};
    std::string depthModelPath;
    int         depthInputSize{256};
    cv::Scalar  depthMean{0.485, 0.456, 0.406};   // ImageNet RGB
    cv::Scalar  depthStd{0.229, 0.224, 0.225};
    int         depthColormap{2};   // cv::COLORMAP_JET = 2; TURBO = 20
    double      depthAlpha{0.55};

    // --- zoom output (optional second video) ---
    bool   enableZoom{true};
    double zoomFactor{1.6};         // crop = bbox * zoomFactor (centered)
    int    zoomOutputWidth{960};
    int    zoomOutputHeight{540};
    double zoomSmoothingAlpha{0.25};// EMA on bbox center+size; lower = smoother

    // --- outputs ---
    std::string overlayVideoPath;
    std::string zoomVideoPath;

    static Config loadFromFile(const std::string& path);
    void resolveRelativePaths(const std::string& configFilePath);

    // Inserts `_<timestamp>` (e.g. _20260503_152200) before the extension
    // of every output path that's set. Inputs are left untouched.
    void stampOutputPaths(const std::string& timestamp);
};

}  // namespace player_highlighter

#endif
