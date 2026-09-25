#include "distance_sensor.h"
#include <fcntl.h>
#include <unistd.h>
#include <cstdint>
#include <stdexcept>

namespace {
constexpr const char* kEchoDevicePath = "/dev/echo_driver";
constexpr double kNoObstacleCm = 999.0;
}

/*
 * trigger 핀 토글은 echo_driver 커널 모듈이 직접 담당한다 (write() 호출을
 * "트리거 펄스 발생" 명령으로 취급). 예전엔 여기서 libgpiod로 /dev/gpiochip0을
 * 직접 열어 trigger 핀을 켰었는데, libgpiod 2.x API가 요구하는 커널 GPIO
 * uAPI v2가 PetaLinux 2017.4의 구형 커널(5.10 미만)에는 없어서 동작하지
 * 않는다. motor_driver와 동일하게 커널 모듈이 GPIO를 전부 소유하는 구조로
 * 맞췄다.
 */
class DistanceSensorDev : public DistanceSensor
{
public:
    DistanceSensorDev(void)
    {
        fd_echo_ = open(kEchoDevicePath, O_RDWR);
        if (fd_echo_ < 0) { throw std::runtime_error("echo_driver open 실패"); }
    }

    ~DistanceSensorDev(void)
    {
        close(fd_echo_);
    }

    double read_cm(void) override
    {
        // write 내용 자체는 의미 없음 - echo_driver 입장에서 write() 호출이
        // 곧 trigger 펄스 발생 명령이다.
        char dummy = 0;
        if (write(fd_echo_, &dummy, sizeof(dummy)) < 0) {
            return kNoObstacleCm;
        }

        int64_t pulse_us;
        ssize_t n = read(fd_echo_, &pulse_us, sizeof(pulse_us));
        if (n != static_cast<ssize_t>(sizeof(pulse_us))) {
            return kNoObstacleCm;
        }

        return static_cast<double>(pulse_us) * 0.017;
    }

private:
    int fd_echo_;
};

extern "C" DistanceSensor* create_distance_sensor() { return new DistanceSensorDev(); }
