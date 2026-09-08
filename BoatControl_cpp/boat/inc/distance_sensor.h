#ifndef DISTANCE_SENSOR_H
#define DISTANCE_SENSOR_H

class DistanceSensor
{
public:
    virtual ~DistanceSensor() = default;
    virtual double read_cm() = 0;
};

#endif