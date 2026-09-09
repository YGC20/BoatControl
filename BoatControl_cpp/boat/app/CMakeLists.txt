cmake_minimum_required(VERSION 3.21)
project(boatcnt_boat CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(USE_HW_MOTOR    "Use ioctl-based motor_driver kernel module" OFF)
option(USE_HW_DISTANCE "Use libgpiod trigger + echo_driver kernel module" OFF)

add_executable(boat
    src/main.cpp
    src/udp_receiver.cpp
)

target_include_directories(boat PRIVATE
    inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../drivers/motor_driver
)

if(USE_HW_MOTOR)
    target_sources(boat PRIVATE src/motor_controller_dev.cpp)
else()
    target_sources(boat PRIVATE src/motor_controller_stub.cpp)
endif()

if(USE_HW_DISTANCE)
    target_sources(boat PRIVATE src/distance_sensor_dev.cpp)
    find_library(GPIOD_LIB gpiod REQUIRED)
    target_link_libraries(boat PRIVATE ${GPIOD_LIB})
else()
    target_sources(boat PRIVATE src/distance_sensor.cpp)
endif()