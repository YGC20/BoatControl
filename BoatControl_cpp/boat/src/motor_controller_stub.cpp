#include "motor_controller.h"
#include <iostream>

class MotorControllerStub : public MotorController
{
public:
    void apply(Command cmd) override
    {
        std::cout << "Status : " << to_string(cmd) << "\n";
        return;
    }
private:
    const char* to_string(Command cmd)
    {
        switch(cmd) {
            case Command::Stop:     return "STOP";
            case Command::Forward:  return "Forward";
            case Command::Backward: return "Backward";
            case Command::Left:     return "Left";
            case Command::Right:    return "Right";
        }
        return "?";
    }
};

extern "C" MotorController* create_motor_controller() { return new MotorControllerStub(); }