cmake_minimum_required(VERSION 3.21)
project(boatcnt_boat CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(USE_GPIOD "Enable libgpiod GPIO backend (PS MIO)" OFF)

add_executable(boat
    src/main.cpp
    src/udp_receiver.cpp
    src/distance_sensor.cpp
)

target_include_directories(boat PRIVATE
    inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
)

if(USE_GPIOD)
    target_sources(boat PRIVATE src/motor_controller_gpio.cpp)
    find_library(GPIOD_LIB gpiod REQUIRED)
    target_link_libraries(boat PRIVATE ${GPIOD_LIB})
else()
    target_sources(boat PRIVATE src/motor_controller_stub.cpp)
endif()