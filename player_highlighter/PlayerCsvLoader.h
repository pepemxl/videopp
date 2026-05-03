#ifndef PLAYER_HIGHLIGHTER_PLAYER_CSV_LOADER_H
#define PLAYER_HIGHLIGHTER_PLAYER_CSV_LOADER_H

#include "Skeleton.h"

#include <opencv2/core.hpp>
#include <array>
#include <map>
#include <string>
#include <vector>

namespace player_highlighter {

struct CsvKeypoint
{
    cv::Point2f pos{0.f, 0.f};
    float       visibility{0.f};
};

struct PlayerFrame
{
    int      frameIdx{-1};
    double   timeMs{0.0};
    bool     hasPose{false};
    cv::Rect bbox{};
    float    personConf{0.f};
    std::array<CsvKeypoint, kNumKeypoints> keypoints{};
};

// Reads the CSV produced by player_tracker. Returns frames in the order
// they appear (one row per row in the CSV, skipping the header).
std::vector<PlayerFrame> loadPlayerCsv(const std::string& path);

}  // namespace player_highlighter

#endif
