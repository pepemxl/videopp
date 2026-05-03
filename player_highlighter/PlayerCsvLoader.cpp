#include "PlayerCsvLoader.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace player_highlighter {

namespace {

constexpr std::array<const char*, kNumKeypoints> kKeypointNames = {
    "nose",
    "left_eye", "right_eye",
    "left_ear", "right_ear",
    "left_shoulder", "right_shoulder",
    "left_elbow",    "right_elbow",
    "left_wrist",    "right_wrist",
    "left_hip",      "right_hip",
    "left_knee",     "right_knee",
    "left_ankle",    "right_ankle",
};

std::vector<std::string> splitCsv(const std::string& line)
{
    std::vector<std::string> out;
    std::string cell;
    std::stringstream ss(line);
    while (std::getline(ss, cell, ',')) out.push_back(cell);
    return out;
}

float toFloat(const std::string& s) { return s.empty() ? 0.f : std::stof(s); }
int   toInt  (const std::string& s) { return s.empty() ? 0   : std::stoi(s); }
double toDouble(const std::string& s) { return s.empty() ? 0.0 : std::stod(s); }

}  // namespace

std::vector<PlayerFrame> loadPlayerCsv(const std::string& path)
{
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Cannot open player CSV: " + path);

    std::string line;
    if (!std::getline(in, line)) {
        throw std::runtime_error("Player CSV is empty: " + path);
    }
    const auto headers = splitCsv(line);

    std::unordered_map<std::string, int> col;
    for (int i = 0; i < static_cast<int>(headers.size()); ++i) {
        col[headers[i]] = i;
    }

    auto need = [&](const std::string& name) {
        auto it = col.find(name);
        if (it == col.end())
            throw std::runtime_error("CSV missing required column: " + name);
        return it->second;
    };

    const int cFrame = need("frame_idx");
    const int cTime  = need("time_ms");
    const int cHas   = need("has_pose");
    const int cBx    = need("bbox_x");
    const int cBy    = need("bbox_y");
    const int cBw    = need("bbox_w");
    const int cBh    = need("bbox_h");
    const int cConf  = need("person_conf");

    std::array<int, kNumKeypoints> cX{}, cY{}, cV{};
    for (int kp = 0; kp < kNumKeypoints; ++kp) {
        const std::string base = kKeypointNames[kp];
        cX[kp] = need(base + "_x");
        cY[kp] = need(base + "_y");
        cV[kp] = need(base + "_v");
    }

    std::vector<PlayerFrame> out;
    out.reserve(256);

    while (std::getline(in, line)) {
        if (line.empty()) continue;
        const auto cells = splitCsv(line);
        if (cells.size() < headers.size()) continue;   // truncated row

        PlayerFrame f;
        f.frameIdx   = toInt(cells[cFrame]);
        f.timeMs     = toDouble(cells[cTime]);
        f.hasPose    = (toInt(cells[cHas]) != 0);
        f.bbox       = cv::Rect(toInt(cells[cBx]), toInt(cells[cBy]),
                                toInt(cells[cBw]), toInt(cells[cBh]));
        f.personConf = toFloat(cells[cConf]);

        for (int kp = 0; kp < kNumKeypoints; ++kp) {
            f.keypoints[kp].pos = cv::Point2f(
                toFloat(cells[cX[kp]]),
                toFloat(cells[cY[kp]]));
            f.keypoints[kp].visibility = toFloat(cells[cV[kp]]);
        }

        out.push_back(f);
    }
    return out;
}

}  // namespace player_highlighter
