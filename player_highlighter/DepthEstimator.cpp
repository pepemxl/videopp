#include "DepthEstimator.h"

#include <opencv2/imgproc.hpp>
#include <stdexcept>

namespace player_highlighter {

DepthEstimator::DepthEstimator(const Config& cfg)
    : m_inputSize(cfg.depthInputSize),
      m_mean(cfg.depthMean),
      m_std(cfg.depthStd)
{
    m_net = cv::dnn::readNetFromONNX(cfg.depthModelPath);
    if (m_net.empty()) {
        throw std::runtime_error("Failed to load depth ONNX model: " + cfg.depthModelPath);
    }
    m_net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
    m_net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
}

cv::Mat DepthEstimator::estimate(const cv::Mat& frame)
{
    // Resize -> RGB float32 -> mean/std normalize -> NCHW blob.
    cv::Mat rgb;
    cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);
    cv::Mat resized;
    cv::resize(rgb, resized, cv::Size(m_inputSize, m_inputSize));
    cv::Mat f;
    resized.convertTo(f, CV_32FC3, 1.0 / 255.0);

    // (x - mean) / std, channel-wise
    std::vector<cv::Mat> chans(3);
    cv::split(f, chans);
    for (int c = 0; c < 3; ++c) {
        chans[c] = (chans[c] - m_mean[c]) / m_std[c];
    }
    cv::merge(chans, f);

    cv::Mat blob = cv::dnn::blobFromImage(f);   // NCHW, no further scaling
    m_net.setInput(blob);

    cv::Mat raw = m_net.forward();
    if (raw.empty()) {
        return cv::Mat::zeros(frame.size(), CV_8UC1);
    }

    // Reshape to 2-D (H_in x W_in) — MiDaS outputs [1, 1, H, W] or [1, H, W].
    cv::Mat depth;
    if (raw.dims == 4)      depth = cv::Mat(raw.size[2], raw.size[3], CV_32F, raw.ptr<float>());
    else if (raw.dims == 3) depth = cv::Mat(raw.size[1], raw.size[2], CV_32F, raw.ptr<float>());
    else                    depth = raw;

    // Upscale to the original frame resolution and normalize to 0..255.
    cv::Mat resizedDepth;
    cv::resize(depth, resizedDepth, frame.size(), 0, 0, cv::INTER_CUBIC);

    double mn = 0, mx = 0;
    cv::minMaxLoc(resizedDepth, &mn, &mx);
    cv::Mat out;
    if (mx - mn < 1e-6) {
        out = cv::Mat::zeros(frame.size(), CV_8UC1);
    } else {
        resizedDepth.convertTo(out, CV_8UC1, 255.0 / (mx - mn), -255.0 * mn / (mx - mn));
    }
    return out;
}

}  // namespace player_highlighter
