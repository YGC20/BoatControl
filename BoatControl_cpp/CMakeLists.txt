cmake_minimum_required(VERSION 3.20)
project(BoatControl_cpp)

option(BUILD_VISION "Build the vision (laptop) app" ON)

include(CMakeDependentOption)
cmake_dependent_option(BUILD_BOAT "Build the boat (Zynq) app" ON "NOT WIN32" OFF)

message(STATUS "BUILD_VISION=${BUILD_VISION}, BUILD_BOAT=${BUILD_BOAT}")

if(NOT BUILD_VISION AND NOT BUILD_BOAT)
    message(FATAL_ERROR "빌드할 대상이 없습니다")
endif()

if(BUILD_VISION)
    add_subdirectory(vision)
endif()

if(BUILD_BOAT)
    add_subdirectory(boat/app)
endif()
