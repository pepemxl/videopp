#ifndef FILTERS_H
#define FILTERS_H

#include <opencv2/opencv.hpp>
#include <opencv2/photo.hpp>
#include <QString>
#include <vector>

namespace Filters {

enum Type {
    None = 0,
    Grayscale,
    GaussianBlur,
    Sharpen,
    MedianBlur,
    BilateralFilter,
    Sobel,
    Laplacian,
    Canny,
    Sepia,
    Vignette,
    Stylization,
    DetailEnhance,
    Invert,
    Count
};

inline QString displayName(int type)
{
    switch (type) {
    case None:            return QStringLiteral("None");
    case Grayscale:       return QStringLiteral("Grayscale");
    case GaussianBlur:    return QStringLiteral("Gaussian Blur");
    case Sharpen:         return QStringLiteral("Sharpen");
    case MedianBlur:      return QStringLiteral("Median Blur");
    case BilateralFilter: return QStringLiteral("Bilateral Filter");
    case Sobel:           return QStringLiteral("Sobel");
    case Laplacian:       return QStringLiteral("Laplacian");
    case Canny:           return QStringLiteral("Canny");
    case Sepia:           return QStringLiteral("Sepia");
    case Vignette:        return QStringLiteral("Vignette");
    case Stylization:     return QStringLiteral("Stylization");
    case DetailEnhance:   return QStringLiteral("Detail Enhance");
    case Invert:          return QStringLiteral("Invert");
    default:              return QString();
    }
}

// Applies the filter in-place. Output channel count may differ from input
// (Grayscale, Sobel, Laplacian, Canny -> 1 channel).
inline void apply(int type, cv::Mat &frame)
{
    if (frame.empty() || type == None) return;

    switch (type) {
    case Grayscale: {
        if (frame.channels() == 3) {
            cv::Mat g;
            cv::cvtColor(frame, g, cv::COLOR_BGR2GRAY);
            frame = g;
        }
        return;
    }
    case GaussianBlur: {
        cv::GaussianBlur(frame, frame, cv::Size(15, 15), 0);
        return;
    }
    case Sharpen: {
        cv::Mat kernel = (cv::Mat_<float>(3, 3) <<
                          0, -1,  0,
                         -1,  5, -1,
                          0, -1,  0);
        cv::filter2D(frame, frame, -1, kernel);
        return;
    }
    case MedianBlur: {
        cv::medianBlur(frame, frame, 7);
        return;
    }
    case BilateralFilter: {
        cv::Mat src = frame.clone();   // bilateralFilter requires src != dst
        cv::bilateralFilter(src, frame, 9, 75, 75);
        return;
    }
    case Sobel: {
        cv::Mat gray;
        if (frame.channels() == 3) cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        else gray = frame;
        cv::Mat gx, gy;
        cv::Sobel(gray, gx, CV_16S, 1, 0, 3);
        cv::Sobel(gray, gy, CV_16S, 0, 1, 3);
        cv::Mat agx, agy, mag;
        cv::convertScaleAbs(gx, agx);
        cv::convertScaleAbs(gy, agy);
        cv::addWeighted(agx, 0.5, agy, 0.5, 0, mag);
        frame = mag;
        return;
    }
    case Laplacian: {
        cv::Mat gray, lap;
        if (frame.channels() == 3) cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        else gray = frame;
        cv::Laplacian(gray, lap, CV_16S, 3);
        cv::convertScaleAbs(lap, lap);
        frame = lap;
        return;
    }
    case Canny: {
        cv::Mat gray, edges;
        if (frame.channels() == 3) cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        else gray = frame;
        cv::Canny(gray, edges, 100, 200);
        frame = edges;
        return;
    }
    case Sepia: {
        if (frame.channels() != 3) return;
        cv::Mat sepiaKernel = (cv::Mat_<float>(3, 3) <<
                               0.272f, 0.534f, 0.131f,
                               0.349f, 0.686f, 0.168f,
                               0.393f, 0.769f, 0.189f);
        cv::Mat tmp;
        cv::transform(frame, tmp, sepiaKernel);
        frame = tmp;
        return;
    }
    case Vignette: {
        if (frame.channels() != 3) return;
        const int rows = frame.rows;
        const int cols = frame.cols;
        cv::Mat kx = cv::getGaussianKernel(cols, cols * 0.4);
        cv::Mat ky = cv::getGaussianKernel(rows, rows * 0.4);
        cv::Mat kernel = ky * kx.t();
        cv::Mat mask;
        cv::normalize(kernel, mask, 0, 1, cv::NORM_MINMAX);
        std::vector<cv::Mat> chs;
        cv::split(frame, chs);
        for (auto &c : chs) {
            cv::Mat cf;
            c.convertTo(cf, CV_32F);
            cf = cf.mul(mask);
            cf.convertTo(c, c.type());
        }
        cv::merge(chs, frame);
        return;
    }
    case Stylization: {
        if (frame.channels() != 3) return;
        cv::Mat src = frame.clone();
        cv::stylization(src, frame);
        return;
    }
    case DetailEnhance: {
        if (frame.channels() != 3) return;
        cv::Mat src = frame.clone();
        cv::detailEnhance(src, frame);
        return;
    }
    case Invert: {
        cv::bitwise_not(frame, frame);
        return;
    }
    default:
        return;
    }
}

}  // namespace Filters

#endif  // FILTERS_H
