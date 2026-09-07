#include <opencv2/opencv.hpp>
#include <algorithm>
#include <cstdio>
#include <iostream>
#include <memory>

#include "detector.h"
#include "udp_sender.h"
#include "detection_packet.h"

extern "C" Detector* create_detector();

int main(int argc, char** argv)
{
    std::string host = "127.0.0.1";
    uint16_t    port = dsu::kDefaultBoatPort;
    int         camera_index = 0;

    for(int i=1; i<argc; ++i) {
        std::string arg = argv[i];
        if(arg == "--host" && i+1 < argc) { host = argv[++i]; }
    }

    cv::VideoCapture cap(camera_index);
    if(!cap.isOpened()) {
        std::cerr << "카메라를 열 수 없습니다.: " << camera_index << "\n";
        return 1;
    }

    std::unique_ptr<Detector> det(create_detector());
    if(!det->load("model/yolov8n.onnx")) {
        std::cerr << "모델 로드 실패\n";
        return 1;
    }
    

    UdpSender sender(host, port);

    std::uint32_t frame_id = 0;
    cv::Mat frame;
    while(cap.read(frame)) {
        auto results = det->infer(frame);

        dsu::DetectionPacket packet{};
        packet.frame_width      = static_cast<std::uint16_t>(frame.cols);
        packet.frame_height     = static_cast<std::uint16_t>(frame.rows);
        packet.frame_id         = frame_id++;
        packet.num_detections   = static_cast<std::uint16_t>(
            std::min<size_t>(results.size(), dsu::kMaxDetections)
        );

        for(std::uint16_t i=0; i<packet.num_detections; ++i) {
            packet.detections[i].class_id   = results[i].class_id;
            packet.detections[i].score      = results[i].score;
            packet.detections[i].x1         = results[i].x1;
            packet.detections[i].y1         = results[i].y1;
            packet.detections[i].x2         = results[i].x2;
            packet.detections[i].y2         = results[i].y2;
        }

        sender.send(packet);

        for(std::uint16_t i=0; i<packet.num_detections; ++i) {
            const auto& r = results[i];
            cv::Rect box(
                cv::Point(static_cast<int>(r.x1), static_cast<int>(r.y1)),
                cv::Point(static_cast<int>(r.x2), static_cast<int>(r.y2))
            );
            cv::rectangle(frame, box, cv::Scalar(0, 255, 0), 2);

            char label[64];
            std::snprintf(label, sizeof(label), "cls:%d %.2f", r.class_id, r.score);
            int baseline = 0;
            cv::Size textSize = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseline);
            cv::Point textOrg(box.x, std::max(box.y - 5, textSize.height));
            cv::rectangle(frame,
                textOrg + cv::Point(0, baseline),
                textOrg + cv::Point(textSize.width, -textSize.height),
                cv::Scalar(0, 255, 0), cv::FILLED);
            cv::putText(frame, label, textOrg, cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0), 1);
        }

        cv::imshow("vision", frame);
        int key = cv::waitKey(1);
        if(key == 27 || key == 'q') { // ESC 또는 q 키로 종료
            break;
        }
        if(cv::getWindowProperty("vision", cv::WND_PROP_VISIBLE) < 1) { // 창 X 버튼으로 닫음
            break;
        }
    }

    cap.release();
    cv::destroyAllWindows();
    return 0;
}