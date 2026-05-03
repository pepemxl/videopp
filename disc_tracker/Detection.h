#ifndef DISC_TRACKER_DETECTION_H
#define DISC_TRACKER_DETECTION_H

#include <opencv2/core.hpp>

namespace disc_tracker {

struct Detection
{
    cv::Rect bbox;
    float    confidence{0.0f};
    int      classId{-1};

    cv::Point2f center() const
    {
        return { bbox.x + bbox.width  * 0.5f,
                 bbox.y + bbox.height * 0.5f };
    }
};

}  // namespace disc_tracker

#endif
