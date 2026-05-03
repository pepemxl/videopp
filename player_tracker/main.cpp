#include "Config.h"
#include "PlayerTracker.h"
#include "Skeleton.h"

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
        "Standalone player-tracking subservice. Reads a YAML/JSON config\n"
        "that points at a video plus a marker file, runs YOLOv8/11-pose\n"
        "via OpenCV DNN per frame, locks onto the player marked in the\n"
        "marker file, and writes a per-frame skeleton CSV (plus an\n"
        "optional annotated overlay video).\n"
        "\n"
        "Tracking window: from the first `player` marker until the first\n"
        "`disc` marker (= release moment), clamped to\n"
        "[min_duration_sec, max_duration_sec] (defaults 4..15s).\n"
        "\n"
        "See player_tracker/configs/example.yaml for the schema.\n";
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
    if (configPath.empty()) { printUsage(argv[0]); return 2; }

    try {
        auto cfg = player_tracker::Config::loadFromFile(configPath);
        cfg.resolveRelativePaths(configPath);
        cfg.stampOutputPaths(nowTimestamp());

        player_tracker::PlayerTracker tracker(cfg);
        const auto report = tracker.run();

        std::cout << "Tracking window: ["
                  << std::fixed << std::setprecision(2)
                  << report.startSec << "s, " << report.endSec << "s]"
                  << "  (duration " << (report.endSec - report.startSec) << "s)\n";
        std::cout << "Frames processed: " << report.frames.size()
                  << "  with pose: " << report.framesWithPose << "\n";
        if (!cfg.csvPath.empty())
            std::cout << "  CSV:     " << cfg.csvPath << "\n";
        if (!cfg.overlayVideoPath.empty())
            std::cout << "  Overlay: " << cfg.overlayVideoPath << "\n";

        if (!report.frames.empty()) {
            std::cout << "\nKeypoint visibility (priority joints first):\n";
            std::cout << std::fixed << std::setprecision(1);
            const size_t total = report.frames.size();
            auto pct = [&](int kp) {
                return 100.0 * static_cast<double>(report.kpVisibleFrames[kp]) /
                       static_cast<double>(total);
            };
            for (int kp : player_tracker::kPriorityKeypoints) {
                std::cout << "  " << player_tracker::kKeypointNames[kp]
                          << ": " << report.kpVisibleFrames[kp] << "/" << total
                          << " (" << pct(kp) << "%)\n";
            }
            std::cout << "\nOther joints:\n";
            for (int kp = 0; kp < player_tracker::kNumKeypoints; ++kp) {
                bool isPrio = false;
                for (int p : player_tracker::kPriorityKeypoints)
                    if (p == kp) { isPrio = true; break; }
                if (isPrio) continue;
                std::cout << "  " << player_tracker::kKeypointNames[kp]
                          << ": " << report.kpVisibleFrames[kp] << "/" << total
                          << " (" << pct(kp) << "%)\n";
            }
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[error] " << e.what() << "\n";
        return 1;
    }
}
