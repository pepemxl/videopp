#ifndef PLAYER_TRACKER_MARKERS_H
#define PLAYER_TRACKER_MARKERS_H

#include <opencv2/core.hpp>
#include <string>
#include <vector>

namespace player_tracker {

struct Marker
{
    enum class Type { Player, Disc, Other };
    Type        type{Type::Other};
    cv::Point2f pos{0.f, 0.f};   // original-frame pixels
    double      timeSec{0.0};
};

struct MarkerSet
{
    std::string         videoPath;
    std::vector<Marker> markers;

    // First player marker, ordered by time. Throws if none exist.
    Marker firstPlayerMarker() const;

    // First disc marker — interpreted as the disc-release moment, the
    // signal to stop player tracking. Throws if none exist.
    Marker firstDiscMarker() const;

    static MarkerSet loadFromFile(const std::string& path);
};

}  // namespace player_tracker

#endif
