#include "Config.h"
#include "Highlighter.h"

#include <chrono>
#include <ctime>
#include <exception>
#include <iomanip>
#include <iostream>
#include <string>

namespace {

// Local-time timestamp for output filenames: YYYYMMDD_HHMMSS.
std::string nowTimestamp()
{
    const auto now = std::chrono::system_clock::now();
    const auto t   = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tm);
    return buf;
}

}  // namespace

namespace {

void printUsage(const char* prog)
{
    std::cout <<
        "Usage: " << prog << " --config <path-to-config>\n"
        "\n"
        "Standalone player-highlighter subservice. Consumes a video plus\n"
        "the per-frame skeleton CSV emitted by player_tracker, segments\n"
        "the player with GrabCut seeded by the skeleton joints, and\n"
        "writes:\n"
        "  - an overlay video at original size with the chosen highlight\n"
        "    style (outline | dim | silhouette | depth_overlay)\n"
        "  - optionally a zoomed crop video centred on the smoothed bbox\n"
        "    for analytics review at higher effective resolution\n"
        "\n"
        "When highlight_mode = depth_overlay (or enable_depth = 1), a\n"
        "MiDaS ONNX model is required (depth_model_path).\n"
        "\n"
        "See player_highlighter/configs/example.yaml for the schema.\n";
}

}  // namespace

int main(int argc, char** argv)
{
    std::string configPath;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if ((a == "--config" || a == "-c") && i + 1 < argc) {
            configPath = argv[++i];
        } else if (a == "-h" || a == "--help") {
            printUsage(argv[0]); return 0;
        } else {
            std::cerr << "Unknown argument: " << a << "\n\n";
            printUsage(argv[0]); return 2;
        }
    }
    if (configPath.empty()) { printUsage(argv[0]); return 2; }

    try {
        auto cfg = player_highlighter::Config::loadFromFile(configPath);
        cfg.resolveRelativePaths(configPath);
        cfg.stampOutputPaths(nowTimestamp());

        player_highlighter::Highlighter h(cfg);
        const auto rep = h.run();

        std::cout << std::fixed << std::setprecision(1);
        std::cout << "Frames processed: " << rep.framesIn << "\n";
        if (rep.framesIn > 0) {
            auto pct = [&](size_t n) {
                return 100.0 * static_cast<double>(n) /
                       static_cast<double>(rep.framesIn);
            };
            std::cout << "  with pose:     " << rep.framesWithPose
                      << "  (" << pct(rep.framesWithPose) << "%)\n";
            std::cout << "  segmented:     " << rep.framesSegmented
                      << "  (" << pct(rep.framesSegmented) << "%)\n";
            if (cfg.enableDepth) {
                std::cout << "  depth-mapped:  " << rep.framesDepth
                          << "  (" << pct(rep.framesDepth) << "%)\n";
            }
            if (cfg.enableZoom && !cfg.zoomVideoPath.empty()) {
                std::cout << "  zoom frames:   " << rep.framesZoomed
                          << "  (" << pct(rep.framesZoomed) << "%)\n";
            }
        }
        if (!cfg.overlayVideoPath.empty())
            std::cout << "  Overlay: " << cfg.overlayVideoPath << "\n";
        if (cfg.enableZoom && !cfg.zoomVideoPath.empty())
            std::cout << "  Zoom:    " << cfg.zoomVideoPath << "\n";

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[error] " << e.what() << "\n";
        return 1;
    }
}
