cmake_minimum_required(VERSION 3.21)
project(boatcnt_vision CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(USE_ORT "Enable ONNX Runtime backend" OFF)

find_package(OpenCV REQUIRED)

add_executable(vision
    src/main.cpp
    src/udp_sender.cpp
    inc/detector.h
    inc/udp_sender.h
)

target_include_directories(vision PRIVATE
    inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
)

target_link_libraries(vision PRIVATE ${OpenCV_LIBS})

if(WIN32)
    target_link_libraries(vision PRIVATE ws2_32)
endif()

if(USE_ORT)
    message(STATUS "Building vision with ONNX Runtime backend")
    if(NOT DEFINED ONNXRUNTIME_ROOT)
        message(FATAL_ERROR "Please set -DONNXRUNTIME_ROOT=path/to/onnxruntime")
    endif()

    target_include_directories(vision PRIVATE "${ONNXRUNTIME_ROOT}/include")

    add_library(onnxruntime SHARED IMPORTED)
    set_target_properties(onnxruntime PROPERTIES 
        IMPORTED_IMPLIB   "${ONNXRUNTIME_ROOT}/lib/onnxruntime.lib"
        IMPORTED_LOCATION "${ONNXRUNTIME_ROOT}/lib/onnxruntime.dll"
    )
    target_link_libraries(vision PRIVATE onnxruntime)

    target_sources(vision PRIVATE src/detector_onnxruntime.cpp)

    # main.cpp이 "model/yolov8n.onnx" 상대경로로 모델을 불러오므로,
    # 실행 파일과 같은 폴더에 model/ 폴더를 자동으로 복사해둔다.
    # (터미널이 아닌 탐색기 더블클릭 실행 시 작업 디렉터리가 exe 위치가 되기 때문)
    add_custom_command(TARGET vision POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/../../model"
            "$<TARGET_FILE_DIR:vision>/model"
    )
else()
    target_sources(vision PRIVATE src/detector_stub.cpp)
endif()

# 실행에 필요한 OpenCV / ONNX Runtime 공유 라이브러리(.dll)를
# 빌드할 때마다 vision.exe와 같은 폴더로 자동 복사
add_custom_command(TARGET vision POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:vision>
        $<TARGET_FILE_DIR:vision>
    COMMAND_EXPAND_LISTS
)
