#pragma once

#ifndef DETECTOR_H
#define DETECTOR_H

#include <opencv2/core.hpp>
#include <string>
#include <vector>

struct Detection {
    int     class_id{};
    float   score{};
    float   x1{}, y1{}, x2{}, y2{};
};

class Detector {
public:
    virtual ~Detector() = default;
    virtual bool load(const std::string& model_path) = 0;
    virtual std::vector<Detection> infer(const cv::Mat& frame) = 0;
};

#endif // DETECTOR_H