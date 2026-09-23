#include "distance_sensor.h"
#include <gpiod.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {
constexpr int kGpioChip = 0;
constexpr unsigned kPinTrigger = 5;

constexpr const char* kEchoDevicePath = "/dev/echo_driver";

constexpr double kNoObstacleCm = 999.0;
}

class DistanceSensorDev : public DistanceSensor
{
public:
    DistanceSensorDev(void)
    {
        const std::string chip_path = "/dev/gpiochip" + std::to_string(kGpioChip);
        chip_ = gpiod_chip_open(chip_path.c_str());
        if (chip_ == nullptr) { throw std::runtime_error("GPIO Chip 생성 실패"); }

        gpiod_line_settings* settings = gpiod_line_settings_new();
        if (settings == nullptr) {
            gpiod_chip_close(chip_);
            throw std::runtime_error("Line Settings 생성 실패");
        }
        gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
        gpiod_line_settings_set_output_value(settings, GPIOD_LINE_VALUE_INACTIVE);

        gpiod_line_config* line_cfg = gpiod_line_config_new();
        if (line_cfg == nullptr) {
            gpiod_line_settings_free(settings);
            gpiod_chip_close(chip_);
            throw std::runtime_error("Line Config 생성 실패");
        }

        const unsigned int offsets[] = { kPinTrigger };
        if (gpiod_line_config_add_line_settings(line_cfg, offsets, 1, settings) != 0) {
            gpiod_line_config_free(line_cfg);
            gpiod_line_settings_free(settings);
            gpiod_chip_close(chip_);
            throw std::runtime_error("Line Config 설정 실패");
        }

        gpiod_request_config* req_cfg = gpiod_request_config_new();
        if (req_cfg == nullptr) {
            gpiod_line_config_free(line_cfg);
            gpiod_line_settings_free(settings);
            gpiod_chip_close(chip_);
            throw std::runtime_error("Request Config 생성 실패");
        }
        gpiod_request_config_set_consumer(req_cfg, "boat_trigger");

        request_ = gpiod_chip_request_lines(chip_, req_cfg, line_cfg);

        // 요청이 끝나면 settings/config 객체는 더 필요 없음 (request_에 결과가 복사됨)
        gpiod_request_config_free(req_cfg);
        gpiod_line_config_free(line_cfg);
        gpiod_line_settings_free(settings);

        if (request_ == nullptr) {
            gpiod_chip_close(chip_);
            throw std::runtime_error("Trigger Line 요청 실패");
        }

        fd_echo_ = open(kEchoDevicePath, O_RDONLY);
        if (fd_echo_ < 0) {
            gpiod_line_request_release(request_);
            gpiod_chip_close(chip_);
            throw std::runtime_error("echo_driver open 실패");
        }
    }

    ~DistanceSensorDev(void)
    {
        close(fd_echo_);
        gpiod_line_request_release(request_);
        gpiod_chip_close(chip_);
    }

    double read_cm(void) override
    {
        gpiod_line_request_set_value(request_, kPinTrigger, GPIOD_LINE_VALUE_ACTIVE);
        usleep(10);
        gpiod_line_request_set_value(request_, kPinTrigger, GPIOD_LINE_VALUE_INACTIVE);

        int64_t pulse_us;
        ssize_t n = read(fd_echo_, &pulse_us, sizeof(pulse_us));
        if (n != static_cast<ssize_t>(sizeof(pulse_us))) {
            return kNoObstacleCm;
        }

        return static_cast<double>(pulse_us) * 0.017;
    }

private:
    gpiod_chip* chip_;
    gpiod_line_request* request_;
    int fd_echo_;
};

extern "C" DistanceSensor* create_distance_sensor() { return new DistanceSensorDev(); }
