#include "Markers.h"

#include <opencv2/core/persistence.hpp>
#include <algorithm>
#include <stdexcept>

namespace player_tracker {

namespace {

const Marker* firstOfType(const std::vector<Marker>& markers, Marker::Type type)
{
    const Marker* best = nullptr;
    for (const auto& m : markers) {
        if (m.type != type) continue;
        if (!best || m.timeSec < best->timeSec) best = &m;
    }
    return best;
}

}  // namespace

Marker MarkerSet::firstPlayerMarker() const
{
    const Marker* m = firstOfType(markers, Marker::Type::Player);
    if (!m) throw std::runtime_error("Marker file has no `player` markers");
    return *m;
}

Marker MarkerSet::firstDiscMarker() const
{
    const Marker* m = firstOfType(markers, Marker::Type::Disc);
    if (!m) throw std::runtime_error("Marker file has no `disc` markers");
    return *m;
}

MarkerSet MarkerSet::loadFromFile(const std::string& path)
{
    cv::FileStorage fs(path, cv::FileStorage::READ);
    if (!fs.isOpened()) {
        throw std::runtime_error("Cannot open markers file: " + path);
    }

    MarkerSet ms;
    if (!fs["video"].empty()) fs["video"] >> ms.videoPath;

    cv::FileNode arr = fs["markers"];
    if (arr.empty() || !arr.isSeq()) return ms;

    for (const auto& node : arr) {
        Marker m;
        std::string type = "other";
        if (!node["type"].empty()) node["type"] >> type;
        if      (type == "player") m.type = Marker::Type::Player;
        else if (type == "disc")   m.type = Marker::Type::Disc;
        else                       m.type = Marker::Type::Other;

        double x = 0.0, y = 0.0, t = 0.0;
        if (!node["x"].empty())    node["x"]    >> x;
        if (!node["y"].empty())    node["y"]    >> y;
        if (!node["time"].empty()) node["time"] >> t;
        m.pos     = cv::Point2f(static_cast<float>(x), static_cast<float>(y));
        m.timeSec = t;
        ms.markers.push_back(m);
    }
    return ms;
}

}  // namespace player_tracker
