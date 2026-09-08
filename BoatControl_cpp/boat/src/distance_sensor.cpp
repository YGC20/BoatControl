#include "distance_sensor.h"

class DummyDistanceSensor : public DistanceSensor
{
public:
    explicit DummyDistanceSensor(double fixed_cm) : fixed_cm_(fixed_cm) {}
    double read_cm(void) override
    {
        return fixed_cm_;
    }
private:
    double fixed_cm_;
};

extern "C" DistanceSensor* create_distance_sensor() { return new DummyDistanceSensor(100.0); }