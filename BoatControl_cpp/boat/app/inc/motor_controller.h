#ifndef MOTOR_CONTROLLER_H
#define MOTOR_CONTROLLER_H

enum class Command { Stop, Forward, Backward, Left, Right };

class MotorController
{
public:
    virtual ~MotorController(void) = default;
    virtual void apply(Command) = 0;
};

#endif