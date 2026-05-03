#include "YoloDetector.h"

#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace disc_tracker {

YoloDetector::YoloDetector(const Config& cfg)
    : m_inputSize(cfg.inputSize),
      m_classIds(cfg.classIds),
      m_confThreshold(cfg.confThreshold),
      m_nmsThreshold(cfg.nmsThreshold)
{
    m_net = cv::dnn::readNetFromONNX(cfg.modelPath);
    if (m_net.empty()) {
        throw std::runtime_error("Failed to load ONNX model: " + cfg.modelPath);
    }
    m_net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
    m_net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
}

std::vector<Detection> YoloDetector::detect(const cv::Mat& frame)
{
    const int origW = frame.cols;
    const int origH = frame.rows;

    // Letterbox to a square m_inputSize canvas (preserve aspect ratio).
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

    // Ultralytics YOLOv8/11 ONNX export shape is [1, 4 + numClasses, numAnchors].
    // Take a 2-D view of the underlying data, then transpose to [anchors, 4+nc].
    if (output.dims == 3) {
        const int dim1 = output.size[1];   // 4 + numClasses
        const int dim2 = output.size[2];   // numAnchors
        output = cv::Mat(dim1, dim2, CV_32F,
                         const_cast<void*>(static_cast<const void*>(output.ptr<float>())))
                     .clone();
        cv::transpose(output, output);     // [anchors, 4+nc]
    } else if (output.dims == 2 && output.cols < output.rows) {
        cv::transpose(output, output);
    }

    const int numAttrs   = output.cols;
    const int numClasses = numAttrs - 4;
    if (numClasses <= 0) {
        throw std::runtime_error(
            "Unexpected YOLO output shape — expected [N, 4 + numClasses]. "
            "Ensure the ONNX model is an Ultralytics YOLOv8/11 detection export.");
    }

    std::vector<cv::Rect>  boxes;
    std::vector<float>     scores;
    std::vector<int>       classIdxs;
    boxes.reserve(64);
    scores.reserve(64);
    classIdxs.reserve(64);

    for (int i = 0; i < output.rows; ++i) {
        const float* row = output.ptr<float>(i);

        float maxScore = 0.0f;
        int   maxClass = -1;
        for (int c = 0; c < numClasses; ++c) {
            const float s = row[4 + c];
            if (s > maxScore) { maxScore = s; maxClass = c; }
        }
        if (maxScore < m_confThreshold) continue;

        if (!m_classIds.empty() &&
            std::find(m_classIds.begin(), m_classIds.end(), maxClass) == m_classIds.end()) {
            continue;
        }

        const float cx = row[0], cy = row[1], w = row[2], h = row[3];
        const float x  = (cx - w * 0.5f - padX) / r;
        const float y  = (cy - h * 0.5f - padY) / r;
        const float bw = w / r;
        const float bh = h / r;

        boxes.emplace_back(static_cast<int>(std::round(x)),
                           static_cast<int>(std::round(y)),
                           static_cast<int>(std::round(bw)),
                           static_cast<int>(std::round(bh)));
        scores.push_back(maxScore);
        classIdxs.push_back(maxClass);
    }

    std::vector<int> keep;
    cv::dnn::NMSBoxes(boxes, scores, m_confThreshold, m_nmsThreshold, keep);

    std::vector<Detection> result;
    result.reserve(keep.size());
    for (int idx : keep) {
        Detection d;
        d.bbox       = boxes[idx] & cv::Rect(0, 0, origW, origH);
        d.confidence = scores[idx];
        d.classId    = classIdxs[idx];
        if (d.bbox.area() > 0) result.push_back(d);
    }
    return result;
}

}  // namespace disc_tracker
