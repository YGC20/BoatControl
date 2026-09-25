set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)

set(CMAKE_SYSROOT $ENV{HOME}/petalinux-work/boatcontrol/build/tmp/sysroots/plnx_arm)
set(CMAKE_FIND_ROOT_PATH ${CMAKE_SYSROOT})

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Ubuntu의 arm-linux-gnueabihf-gcc는 --sysroot를 헤더 검색에는 반영하지만
# crt 시작파일/libc.so는 자체 내장 경로(/usr/arm-linux-gnueabihf)를 우선 사용한다.
# -B로 우리 sysroot를 더 높은 우선순위로 강제 지정해야 진짜 타겟 glibc(2.23)가 링크된다.
set(_target_flags "-mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard -B${CMAKE_SYSROOT}/usr/lib -B${CMAKE_SYSROOT}/lib")

set(CMAKE_C_FLAGS   "${_target_flags}" CACHE STRING "")
set(CMAKE_CXX_FLAGS "${_target_flags}" CACHE STRING "")

set(CMAKE_EXE_LINKER_FLAGS "${_target_flags} -static-libgcc -static-libstdc++" CACHE STRING "")
