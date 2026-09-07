#include "detector.h"
#include <onnxruntime_cxx_api.h>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <iostream>
#include <array>
#include <codecvt>
#include <locale>

class OrtDetector : public Detector {
public:
    bool load(const std::string& model_path) override {
        try {
            Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "boatcnt");
            env_ = std::move(env);
            Ort::SessionOptions opt;
#ifdef _WIN32
            std::wstring wpath(model_path.begin(), model_path.end());
            session_ = std::make_unique<Ort::Session>(env_, wpath.c_str(), opt);
#else
            session_ = std::make_unique<Ort::Session>(env_, model_path.c_str(), opt);
#endif
            allocator_ = std::make_unique<Ort::AllocatorWithDefaultOptions>();
            std::cout << "[ORT] Loaded: " << model_path << "\n";
            return true;
        } catch (const std::exception& e) {
            std::cerr << "[ORT] Load failed: " << e.what() << "\n";
            return false;
        }
    }

    std::vector<Detection> infer(const cv::Mat& frame) override {
        try{
            LetterboxResult lb = letterboxResize(frame, cv::Size(640,640));
            cv::Mat letterboxed = lb.image;
        
            cv::Mat rgb;
            cv::cvtColor(letterboxed, rgb, cv::COLOR_BGR2RGB);

            cv::Mat floatImg;
            rgb.convertTo(floatImg, CV_32FC3, 1.0/255.0);

            std::vector<cv::Mat> channels(3);
            cv::split(floatImg, channels);

            std::vector<float> inputTensorValues;
            inputTensorValues.reserve(3 * 640 * 640);
            for(int c=0; c<3; ++c) {
                inputTensorValues.insert(inputTensorValues.end(),
                    reinterpret_cast<const float*>(channels[c].datastart),
                    reinterpret_cast<const float*>(channels[c].dataend));
            }

            std::array<int64_t, 4> inputShape{1, 3, 640, 640};
            Ort::MemoryInfo memInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
            Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
                memInfo,
                inputTensorValues.data(), inputTensorValues.size(),
                inputShape.data(), inputShape.size()
            );

            const char* inputNames[] = {"images"};
            const char* outputNames[] = {"output0"};

            std::vector<Ort::Value> outputTensors = session_->Run(
                Ort::RunOptions{nullptr}, inputNames, &inputTensor,
                1, outputNames, 1
            );

            float* outputData = outputTensors[0].GetTensorMutableData<float>();
            auto shape = outputTensors[0].GetTensorTypeAndShapeInfo().GetShape();

            static bool loggedShapeOnce = false;
            if(!loggedShapeOnce) {
                std::cout << "output shape: " << shape[0] << "," << shape[1] << "," << shape[2] << "\n";
                loggedShapeOnce = true;
            }

            constexpr float kScoreThreshold = 0.25f;

            std::vector<Detection> results;
            results.reserve(static_cast<size_t>(shape[1]));

            for(int64_t i=0; i<shape[1]; ++i) {
                const float* row = outputData + i * shape[2];
                float x1 = row[0], y1 = row[1], x2 = row[2], y2 = row[3];
                float score = row[4];
                int cls = static_cast<int>(row[5]);

                if(score < kScoreThreshold) { continue; }

                Detection d;
                d.class_id  = cls;
                d.score     = score;
                d.x1 = std::clamp((x1 - lb.x_offset) / lb.scale, 0.f, static_cast<float>(frame.cols));
                d.y1 = std::clamp((y1 - lb.y_offset) / lb.scale, 0.f, static_cast<float>(frame.rows));
                d.x2 = std::clamp((x2 - lb.x_offset) / lb.scale, 0.f, static_cast<float>(frame.cols));
                d.y2 = std::clamp((y2 - lb.y_offset) / lb.scale, 0.f, static_cast<float>(frame.rows));

                results.push_back(d);
            }

            return results;
        } catch(const std::exception& e) {
            std::cout << "[ORT] Inference failed: \n" << e.what() << "\n";
            return {};
        }
    }
private:
    Ort::Env env_{ORT_LOGGING_LEVEL_WARNING, "boatcnt"};
    std::unique_ptr<Ort::Session> session_;
    std::unique_ptr<Ort::AllocatorWithDefaultOptions> allocator_;

    struct LetterboxResult {
        cv::Mat image;
        float   scale;
        int     x_offset, y_offset;
    };

    LetterboxResult letterboxResize(const cv::Mat& src, const cv::Size& targetSize, 
        const cv::Scalar& bgColor = cv::Scalar(0,0,0)
    )
    {
        int src_w = src.cols, src_h = src.rows;
        int target_w = targetSize.width, target_h = targetSize.height;

        float scale = std::min((float)target_w/src_w, (float)target_h/src_h);

        int new_w = static_cast<int>(src_w * scale), new_h = static_cast<int>(src_h * scale);

        cv::Mat resizedImg;
        cv::resize(src, resizedImg, cv::Size(new_w, new_h), 0, 0, cv::INTER_LINEAR);

        cv::Mat sandBox = cv::Mat::zeros(targetSize, src.type());
        sandBox.setTo(bgColor);

        int x_offset = (target_w - new_w) / 2;
        int y_offset = (target_h - new_h) / 2;

        cv::Mat roi = sandBox(cv::Rect(x_offset, y_offset, new_w, new_h));
        resizedImg.copyTo(roi);

        return LetterboxResult{sandBox, scale, x_offset, y_offset};
    }
};

extern "C" Detector* create_detector() { return new OrtDetector(); }
