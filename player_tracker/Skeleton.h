#ifndef PLAYER_TRACKER_SKELETON_H
#define PLAYER_TRACKER_SKELETON_H

#include <array>
#include <string_view>
#include <utility>

namespace player_tracker {

// COCO 17-keypoint topology (Ultralytics YOLOv8-pose ordering).
constexpr int kNumKeypoints = 17;

// Keypoint indices (anonymous enum so the name `Keypoint` stays free for
// the per-frame struct in PoseDetector.h).
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

constexpr std::array<std::string_view, kNumKeypoints> kKeypointNames = {
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

// User-priority joints — shoulders, elbows, knees.
constexpr std::array<int, 6> kPriorityKeypoints = {
    KP_LEFT_SHOULDER, KP_RIGHT_SHOULDER,
    KP_LEFT_ELBOW,    KP_RIGHT_ELBOW,
    KP_LEFT_KNEE,     KP_RIGHT_KNEE,
};

// Edges drawn between joints to form the skeleton overlay. Listed in a
// stable order so colors can be assigned per-edge if desired.
constexpr std::array<std::pair<int, int>, 16> kSkeletonEdges = {{
    // arms
    {KP_LEFT_SHOULDER,  KP_LEFT_ELBOW},
    {KP_LEFT_ELBOW,     KP_LEFT_WRIST},
    {KP_RIGHT_SHOULDER, KP_RIGHT_ELBOW},
    {KP_RIGHT_ELBOW,    KP_RIGHT_WRIST},
    // shoulders / torso
    {KP_LEFT_SHOULDER,  KP_RIGHT_SHOULDER},
    {KP_LEFT_SHOULDER,  KP_LEFT_HIP},
    {KP_RIGHT_SHOULDER, KP_RIGHT_HIP},
    {KP_LEFT_HIP,       KP_RIGHT_HIP},
    // legs
    {KP_LEFT_HIP,       KP_LEFT_KNEE},
    {KP_LEFT_KNEE,      KP_LEFT_ANKLE},
    {KP_RIGHT_HIP,      KP_RIGHT_KNEE},
    {KP_RIGHT_KNEE,     KP_RIGHT_ANKLE},
    // head (light)
    {KP_NOSE,           KP_LEFT_EYE},
    {KP_NOSE,           KP_RIGHT_EYE},
    {KP_LEFT_EYE,       KP_LEFT_EAR},
    {KP_RIGHT_EYE,      KP_RIGHT_EAR},
}};

}  // namespace player_tracker

#endif
