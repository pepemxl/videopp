#include "PoseDetector.h"

#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace player_tracker {

PoseDetector::PoseDetector(const Config& cfg)
    : m_inputSize(cfg.inputSize),
      m_personConfThreshold(cfg.personConfThreshold),
      m_nmsThreshold(cfg.nmsThreshold)
{
    m_net = cv::dnn::readNetFromONNX(cfg.modelPath);
    if (m_net.empty()) {
        throw std::runtime_error("Failed to load pose ONNX model: " + cfg.modelPath);
    }
    m_net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
    m_net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
}

std::vector<PoseDetection> PoseDetector::detect(const cv::Mat& frame)
{
    const int origW = frame.cols;
    const int origH = frame.rows;

    // Letterbox to a square input.
    const float r = std::min(static_cast<float>(m_inputSize) / origW,
                             static_cast<float>(m_inputSize) / origH);
    const int newW = static_cast<int>(std::round(origW * r));
    const int newH = static_cast<int>(std::round(origH * r));
    const int padX = (m_inputSize - newW) / 2;
    const int padY = (m_inputSize - newH) / 2;

    cv::Mat resized;
    cv::resize(frame, resized, cv::Size(newW, newH));
    cv::Mat canvas(m_inputSize, m_inputSize, frame.type(), cv::Scalar(114, 114, 114));
    resized.copyTo(canvas(cv::Rect(padX, padY, newW, newH)));

    cv::Mat blob = cv::dnn::blobFromImage(canvas, 1.0 / 255.0,
                                          cv::Size(m_inputSize, m_inputSize),
                                          cv::Scalar(), /*swapRB=*/true, /*crop=*/false);
    m_net.setInput(blob);

    std::vector<cv::Mat> outputs;
    m_net.forward(outputs, m_net.getUnconnectedOutLayersNames());
    if (outputs.empty()) return {};

    cv::Mat output = outputs[0];

    // Output layout from Ultralytics pose export: [1, 4 + 1 + 17*3, anchors]
    // Materialize as 2-D and transpose to [anchors, attrs].
    if (output.dims == 3) {
        const int dim1 = output.size[1];
        const int dim2 = output.size[2];
        output = cv::Mat(dim1, dim2, CV_32F,
                         const_cast<void*>(static_cast<const void*>(output.ptr<float>())))
                     .clone();
        cv::transpose(output, output);
    } else if (output.dims == 2 && output.cols < output.rows) {
        cv::transpose(output, output);
    }

    const int expectedCols = 4 + 1 + kNumKeypoints * 3;
    if (output.cols != expectedCols) {
        throw std::runtime_error(
            "Unexpected pose ONNX output cols=" + std::to_string(output.cols) +
            ", expected " + std::to_string(expectedCols) +
            " (4 box + 1 score + 17*3 keypoints). "
            "Confirm the model is an Ultralytics YOLOv8/11-pose export.");
    }

    std::vector<cv::Rect>  boxes;
    std::vector<float>     scores;
    std::vector<int>       rowIdx;
    boxes.reserve(64);
    scores.reserve(64);
    rowIdx.reserve(64);

    for (int i = 0; i < output.rows; ++i) {
        const float* row = output.ptr<float>(i);
        const float personScore = row[4];
        if (personScore < m_personConfThreshold) continue;

        const float cx = row[0], cy = row[1], w = row[2], h = row[3];
        const float x  = (cx - w * 0.5f - padX) / r;
        const float y  = (cy - h * 0.5f - padY) / r;
        const float bw = w / r;
        const float bh = h / r;

        boxes.emplace_back(static_cast<int>(std::round(x)),
                           static_cast<int>(std::round(y)),
                           static_cast<int>(std::round(bw)),
                           static_cast<int>(std::round(bh)));
        scores.push_back(personScore);
        rowIdx.push_back(i);
    }

    std::vector<int> keep;
    cv::dnn::NMSBoxes(boxes, scores, m_personConfThreshold, m_nmsThreshold, keep);

    std::vector<PoseDetection> result;
    result.reserve(keep.size());
    const cv::Rect frameRect(0, 0, origW, origH);

    for (int k : keep) {
        const int srcRow = rowIdx[k];
        const float* row = output.ptr<float>(srcRow);

        PoseDetection d;
        d.bbox       = boxes[k] & frameRect;
        d.personConf = scores[k];
        if (d.bbox.area() == 0) continue;

        for (int kp = 0; kp < kNumKeypoints; ++kp) {
            const float kx = row[5 + kp * 3 + 0];
            const float ky = row[5 + kp * 3 + 1];
            const float kv = row[5 + kp * 3 + 2];
            d.keypoints[kp].pos = cv::Point2f((kx - padX) / r,
                                              (ky - padY) / r);
            d.keypoints[kp].visibility = kv;
        }
        result.push_back(d);
    }
    return result;
}

}  // namespace player_tracker
