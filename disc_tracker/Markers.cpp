#include "Markers.h"

#include <opencv2/core/persistence.hpp>
#include <algorithm>
#include <stdexcept>

namespace disc_tracker {

std::vector<Marker> MarkerSet::discMarkers() const
{
    std::vector<Marker> out;
    out.reserve(markers.size());
    for (const auto& m : markers) {
        if (m.type == Marker::Type::Disc) out.push_back(m);
    }
    std::sort(out.begin(), out.end(),
              [](const Marker& a, const Marker& b) { return a.timeSec < b.timeSec; });
    return out;
}

MarkerSet MarkerSet::loadFromFile(const std::string& path)
{
    cv::FileStorage fs(path, cv::FileStorage::READ);
    if (!fs.isOpened()) {
        throw std::runtime_error("Cannot open markers file: " + path);
    }

    MarkerSet ms;
    if (!fs["video"].empty())  fs["video"]  >> ms.videoPath;

    cv::FileNode arr = fs["markers"];
    if (arr.empty() || !arr.isSeq()) {
        return ms;   // no markers field -> empty set is fine
    }

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

}  // namespace disc_tracker
