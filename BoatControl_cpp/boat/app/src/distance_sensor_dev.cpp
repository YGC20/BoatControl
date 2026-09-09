#include "distance_sensor.h"
#include <gpiod.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdint>
#include <stdexcept>

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
        chip_ = gpiod_chip_open_by_number(kGpioChip);
        if (chip_ == nullptr) { throw std::runtime_error("GPIO Chip 생성 실패"); }

        trigger_ = gpiod_chip_get_line(chip_, kPinTrigger);
        if (trigger_ == nullptr) {
            gpiod_chip_close(chip_);
            throw std::runtime_error("Trigger Line 연결 실패");
        }

        if (gpiod_line_request_output(trigger_, "boat_trigger", 0) != 0) {
            gpiod_chip_close(chip_);
            throw std::runtime_error("Trigger Request Output 실패");
        }

        fd_echo_ = open(kEchoDevicePath, O_RDONLY);
        if (fd_echo_ < 0) {
            gpiod_line_release(trigger_);
            gpiod_chip_close(chip_);
            throw std::runtime_error("echo_driver open 실패");
        }
    }

    ~DistanceSensorDev(void)
    {
        close(fd_echo_);
        gpiod_line_release(trigger_);
        gpiod_chip_close(chip_);
    }

    double read_cm(void) override
    {
        gpiod_line_set_value(trigger_, 1);
        usleep(10);
        gpiod_line_set_value(trigger_, 0);

        int64_t pulse_us;
        ssize_t n = read(fd_echo_, &pulse_us, sizeof(pulse_us));
        if (n != static_cast<ssize_t>(sizeof(pulse_us))) {
            return kNoObstacleCm;
        }

        return static_cast<double>(pulse_us) * 0.017;
    }

private:
    gpiod_chip* chip_;
    gpiod_line* trigger_;
    int fd_echo_;
};

extern "C" DistanceSensor* create_distance_sensor() { return new DistanceSensorDev(); }