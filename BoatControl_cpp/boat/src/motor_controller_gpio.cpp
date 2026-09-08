#include "motor_pin.h"
#include "motor_controller.h"
#include <gpiod.h>
#include <iostream>
#include <stdexcept>

class MotorControllerGpio : public MotorController
{
public:
    MotorControllerGpio(void)
    {
        chip_ = gpiod_chip_open_by_number(bcc::kGpioChip);
        if(chip_ == nullptr) { throw std::runtime_error("GPIO Chip 생성 실패"); }

        left_fwd_ = gpiod_chip_get_line(chip_, bcc::kPinLeftForward);
        left_bwd_ = gpiod_chip_get_line(chip_, bcc::kPinLeftBackward);
        right_fwd_ = gpiod_chip_get_line(chip_, bcc::kPinRightForward);
        right_bwd_ = gpiod_chip_get_line(chip_, bcc::kPinRightBackward);
        if(left_fwd_ == nullptr || left_bwd_ == nullptr ||
            right_fwd_ == nullptr || right_bwd_ == nullptr) 
        { throw std::runtime_error("GPIO Line 연결 실패"); }

        if( gpiod_line_request_output(left_fwd_, "boat_motor", 0) != 0 ||
            gpiod_line_request_output(left_bwd_, "boat_motor", 0) != 0 ||
            gpiod_line_request_output(right_fwd_, "boat_motor", 0) != 0 ||
            gpiod_line_request_output(right_bwd_, "boat_motor", 0) != 0) 
        { throw std::runtime_error("Request Output 샐패"); }
    }

    ~MotorControllerGpio(void)
    {
        apply(Command::Stop);

        gpiod_line_release(left_fwd_);
        gpiod_line_release(left_bwd_);
        gpiod_line_release(right_fwd_);
        gpiod_line_release(right_bwd_);

        gpiod_chip_close(chip_);
    }

    void apply(Command cmd) override
    {
        switch(cmd) {
        case Command::Stop :
            gpiod_line_set_value(left_fwd_, 0);
            gpiod_line_set_value(left_bwd_, 0);
            gpiod_line_set_value(right_fwd_, 0);
            gpiod_line_set_value(right_bwd_, 0);
            break;
        case Command::Forward :
            gpiod_line_set_value(left_fwd_, 1);
            gpiod_line_set_value(left_bwd_, 0);
            gpiod_line_set_value(right_fwd_, 1);
            gpiod_line_set_value(right_bwd_, 0);
            break;
        case Command::Backward :
            gpiod_line_set_value(left_fwd_, 0);
            gpiod_line_set_value(left_bwd_, 1);
            gpiod_line_set_value(right_fwd_, 0);
            gpiod_line_set_value(right_bwd_, 1);
            break;
        case Command::Left :
            gpiod_line_set_value(left_fwd_, 0);
            gpiod_line_set_value(left_bwd_, 1);
            gpiod_line_set_value(right_fwd_, 1);
            gpiod_line_set_value(right_bwd_, 0);
            break;
        case Command::Right :
            gpiod_line_set_value(left_fwd_, 1);
            gpiod_line_set_value(left_bwd_, 0);
            gpiod_line_set_value(right_fwd_, 0);
            gpiod_line_set_value(right_bwd_, 1);
            break;
        }
    }
private:
    gpiod_chip* chip_;
    gpiod_line* left_fwd_;
    gpiod_line* left_bwd_;
    gpiod_line* right_fwd_;
    gpiod_line* right_bwd_;
};

extern "C" MotorController* create_motor_controller() { return new MotorControllerGpio(); }