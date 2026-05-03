#ifndef PLAYER_HIGHLIGHTER_SKELETON_H
#define PLAYER_HIGHLIGHTER_SKELETON_H

#include <array>
#include <utility>

namespace player_highlighter {

// COCO 17-keypoint topology, mirroring player_tracker's Skeleton.h so this
// service stays standalone (no cross-target #include).
constexpr int kNumKeypoints = 17;

enum : int {
    KP_NOSE = 0,
    KP_LEFT_EYE = 1, KP_RIGHT_EYE = 2,
    KP_LEFT_EAR = 3, KP_RIGHT_EAR = 4,
    KP_LEFT_SHOULDER = 5, KP_RIGHT_SHOULDER = 6,
    KP_LEFT_ELBOW = 7,    KP_RIGHT_ELBOW    = 8,
    KP_LEFT_WRIST = 9,    KP_RIGHT_WRIST    = 10,
    KP_LEFT_HIP = 11,     KP_RIGHT_HIP      = 12,
    KP_LEFT_KNEE = 13,    KP_RIGHT_KNEE     = 14,
    KP_LEFT_ANKLE = 15,   KP_RIGHT_ANKLE    = 16,
};

constexpr std::array<std::pair<int, int>, 12> kSkeletonEdges = {{
    {KP_LEFT_SHOULDER,  KP_LEFT_ELBOW},
    {KP_LEFT_ELBOW,     KP_LEFT_WRIST},
    {KP_RIGHT_SHOULDER, KP_RIGHT_ELBOW},
    {KP_RIGHT_ELBOW,    KP_RIGHT_WRIST},
    {KP_LEFT_SHOULDER,  KP_RIGHT_SHOULDER},
    {KP_LEFT_SHOULDER,  KP_LEFT_HIP},
    {KP_RIGHT_SHOULDER, KP_RIGHT_HIP},
    {KP_LEFT_HIP,       KP_RIGHT_HIP},
    {KP_LEFT_HIP,       KP_LEFT_KNEE},
    {KP_LEFT_KNEE,      KP_LEFT_ANKLE},
    {KP_RIGHT_HIP,      KP_RIGHT_KNEE},
    {KP_RIGHT_KNEE,     KP_RIGHT_ANKLE},
}};

}  // namespace player_highlighter

#endif
