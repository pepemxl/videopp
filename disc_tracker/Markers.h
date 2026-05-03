#ifndef DISC_TRACKER_MARKERS_H
#define DISC_TRACKER_MARKERS_H

#include <opencv2/core.hpp>
#include <string>
#include <vector>

namespace disc_tracker {

struct Marker
{
    enum class Type { Player, Disc, Other };
    Type        type{Type::Other};
    cv::Point2f pos{0.f, 0.f};   // in original-frame pixel coordinates
    double      timeSec{0.0};
};

struct MarkerSet
{
    std::string         videoPath;
    std::vector<Marker> markers;

    // Returns disc markers only, sorted by time ascending.
    std::vector<Marker> discMarkers() const;

    static MarkerSet loadFromFile(const std::string& path);
};

}  // namespace disc_tracker

#endif
