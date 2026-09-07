#include "detector.h"
#include <iostream>

class StubDetector : public Detector {
public:
    bool load(const std::string& model_path) override {
        std::cout << "[STUB] ONNX Runtime not enabled. Model path ignored: " << model_path << "\n";
        return true;
    }
    std::vector<Detection> infer(const cv::Mat& frame) override {
        return {};
    }
};

extern "C" Detector* create_detector() { return new StubDetector(); }
