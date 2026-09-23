#include <memory>
#include <csignal>
#include <cmath>
#include <cstdint>
#include <random>
#include <string>
#include <stdexcept>
#include <iostream>
#include "motor_controller.h"
#include "distance_sensor.h"
#include "detection_packet.h"
#include "udp_receiver.h"

Command returnDir(void);

extern "C" MotorController* create_motor_controller();
extern "C" DistanceSensor* create_distance_sensor();

int parse_int(const std::string& str, int lo, int hi);

namespace {

volatile std::sig_atomic_t g_stop_requested = 0;

void handle_stop_signal(int)
{
    g_stop_requested = 1;
}

} // namespace

int main(int argc, char** argv)
{
    uint16_t port = bcc::kDefaultBoatPort;

    for(int i=1; i<argc; ++i) {
        std::string arg = argv[i];
        if(arg == "--port" && i+1 < argc) {
            std::string value = argv[++i];
            try { 
                port = static_cast<uint16_t>(parse_int(value, 1, 65535));
            } catch(const std::invalid_argument& e) {
                std::cerr << "--port: " << e.what() << "\n"; return 1;
            } catch(const std::out_of_range& e) {
                std::cerr << "--port: " << e.what() << "\n"; return 1;
            }
        }
    }

    std::signal(SIGINT, handle_stop_signal);
    std::signal(SIGTERM, handle_stop_signal);

    std::unique_ptr<MotorController> motor;
    std::unique_ptr<DistanceSensor> distance;
    try {
        motor = std::unique_ptr<MotorController>(create_motor_controller());
        distance = std::unique_ptr<DistanceSensor>(create_distance_sensor());
    } catch(const std::exception& e) {
        std::cerr << "초기화 실패: " << e.what() << "\n";
        return 1;
    }
    UdpReceiver receiver(port, 200);

    while(!g_stop_requested) {
        double dist = distance->read_cm();
        if(dist < 60.0) {
            motor->apply(Command::Stop);
            continue;
        }

        bcc::DetectionPacket packet{};
        if(!receiver.receive(packet) || packet.num_detections == 0) {
            motor->apply(returnDir());
            continue;
        }

        const bcc::DetectionEntry* best = &packet.detections[0];
        for(uint16_t i=1; i<packet.num_detections; ++i) {
            if(best->score < packet.detections[i].score) { best = &packet.detections[i]; }
        }

        float x_center = (best->x1 + best->x2) / 2.0f;
        float frame_center = packet.frame_width / 2.0f;

        if(std::abs(x_center - frame_center) < 80.0f) { motor->apply(Command::Forward); }
        else if(x_center < frame_center) { motor->apply(Command::Left); }
        else { motor->apply(Command::Right); }
    }

    motor->apply(Command::Stop); // 종료 신호로 루프를 빠져나온 직후 반드시 정지
    return 0;
}

Command returnDir(void)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> distrib(1,3);
    switch(distrib(gen)) {
    case 1:     return Command::Forward;
    case 2:     return Command::Left;
    case 3:     return Command::Right;
    default:    return Command::Forward;
    }
}

int parse_int(const std::string& str, int lo, int hi)
{
    std::size_t len = 0;
    int num = std::stoi(str, &len);

    if(len != str.size()) {
        throw std::invalid_argument("invalid integer");
    }
    if(num < lo || num > hi) {
        throw std::out_of_range("integer out of range");
    }
    return num;
}