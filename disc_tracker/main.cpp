#include "Config.h"
#include "DiscTracker.h"

#include <opencv2/core/utility.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
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
        "Standalone disc-trajectory subservice. Reads a YAML/JSON config that\n"
        "points at a video, runs the configured detector (yolo|hough) per\n"
        "frame, smooths the path with a Kalman filter, and writes a CSV path\n"
        "(plus optional annotated overlay video).\n"
        "\n"
        "See disc_tracker/configs/example.yaml for the schema.\n";
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
            printUsage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown argument: " << a << "\n\n";
            printUsage(argv[0]);
            return 2;
        }
    }

    if (configPath.empty()) {
        printUsage(argv[0]);
        return 2;
    }

    try {
        auto cfg = disc_tracker::Config::loadFromFile(configPath);
        cfg.resolveRelativePaths(configPath);
        cfg.stampOutputPaths(nowTimestamp());

        disc_tracker::DiscTracker tracker(cfg);
        const auto report = tracker.run();

        std::cout << "Processed " << report.path.size() << " frames "
                  << "(detections=" << report.detections
                  << ", kalman_bridged=" << report.kalmanBridged
                  << ", anchor=" << report.anchorOnly
                  << ", lost=" << report.lost << ")\n";
        if (!cfg.csvPath.empty())
            std::cout << "  CSV:     " << cfg.csvPath << "\n";
        if (!cfg.overlayVideoPath.empty())
            std::cout << "  Overlay: " << cfg.overlayVideoPath << "\n";

        if (!report.residuals.empty()) {
            double sum = 0.0, maxErr = 0.0;
            size_t matched = 0;
            std::cout << "\nMarker residuals (px error vs. ground-truth):\n";
            std::cout << std::fixed << std::setprecision(2);
            for (const auto& r : report.residuals) {
                std::cout << "  t=" << r.marker.timeSec << "s  "
                          << "marker=(" << r.marker.pos.x << "," << r.marker.pos.y << ")  ";
                if (r.hasMatch) {
                    std::cout << "tracker=(" << r.matched.cx << "," << r.matched.cy << ")  "
                              << "err=" << r.pixelError << " px  "
                              << "(dt=" << r.timeDiffMs << " ms)";
                    sum    += r.pixelError;
                    maxErr  = std::max(maxErr, r.pixelError);
                    ++matched;
                } else {
                    std::cout << "no tracker frame within tolerance";
                }
                std::cout << "\n";
            }
            if (matched > 0) {
                std::cout << "  -> matched " << matched << "/" << report.residuals.size()
                          << " markers, mean=" << (sum / matched)
                          << " px, max=" << maxErr << " px\n";
            }
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[error] " << e.what() << "\n";
        return 1;
    }
}
