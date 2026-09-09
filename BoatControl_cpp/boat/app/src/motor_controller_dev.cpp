#include "motor_controller.h"
#include "motor_ioctl.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdexcept>

class MotorControllerDev : public MotorController
{
public:
    MotorControllerDev(void)
    {
        fd_ = open(MOTOR_DEVICE_PATH, O_RDWR);
        if (fd_ < 0) { throw std::runtime_error("motor_driver open 실패"); }
    }

    ~MotorControllerDev(void)
    {
        apply(Command::Stop);
        close(fd_);
    }

    void apply(Command cmd) override
    {
        ioctl(fd_, MOTOR_SET_DIR, static_cast<int>(cmd));
    }

private:
    int fd_;
};

extern "C" MotorController* create_motor_controller() { return new MotorControllerDev(); }