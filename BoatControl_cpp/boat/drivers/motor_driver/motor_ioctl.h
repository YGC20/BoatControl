#ifndef MOTOR_IOCTL_H
#define MOTOR_IOCTL_H

#include <linux/ioctl.h>

enum motor_direction {
    MOTOR_STOP     = 0,
    MOTOR_FORWARD  = 1,
    MOTOR_BACKWARD = 2,
    MOTOR_LEFT     = 3,
    MOTOR_RIGHT    = 4,
};

#define MOTOR_IOC_MAGIC 'M'
#define MOTOR_SET_DIR   _IOW(MOTOR_IOC_MAGIC, 0, int)

#define MOTOR_DEVICE_NAME "motor_driver"
#define MOTOR_DEVICE_PATH "/dev/" MOTOR_DEVICE_NAME

#endif /* MOTOR_IOCTL_H */
