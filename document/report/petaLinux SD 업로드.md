# PetaLinux 개발 환경 구축 진행 상황 (Zybo Z7-10)

마지막 업데이트: 2026-09-25

## 결정된 방향
- 보드: **Zybo Z7-10** (Zynq-7010)
- Digilent 공식 BSP 기준 → **PetaLinux 2017.4** 고정 사용
- 호스트: 네이티브 Ubuntu 26.04 PC (용량 278GB 여유)
- Docker (Ubuntu 16.04 base) 컨테이너로 격리해서 진행 중
  - 컨테이너: `docker start -ai petalinux-2017.4` (재진입), 인터랙티브 명령은 `docker exec -u petalinux -it petalinux-2017.4 bash`로 접속 (su -는 screen pty 에러 남)
  - 마운트: 호스트 `~/petalinux-work` ↔ 컨테이너 `/workspace`
  - PetaLinux 설치 경로: `/workspace/tools/opt/pkg/petalinux`
  - 프로젝트 경로: `/workspace/boatcontrol`
  - 일반 사용자 `petalinux` 계정으로 작업 (root 설치 불가 이슈로 생성)
  - **PetaLinux 명령어 쓰려면 매 세션 `source /workspace/tools/opt/pkg/petalinux/settings.sh` 필요** (안 하면 `petalinux-package: command not found`)
  - GTK3 이슈 때문에 매 세션 `export SWT_GTK3=0` 필요 (아래 참고, ~/.bashrc에 추가 권장)
  - **주의**: `petalinux-work` 마운트와 `BoatControl` 앱 저장소(`~/YGC/soloProject/BoatControl`)는 서로 다른 호스트 경로라 컨테이너 안에서 앱 저장소가 안 보임 — 드라이버 소스나 앱 소스를 컨테이너에서 써야 할 땐 호스트 터미널(컨테이너 밖)에서 `~/petalinux-work/` 밑으로 `cp`로 옮겨야 함 (`/workspace`와 동일한 마운트라 컨테이너에서 바로 보임)
- SD카드: 노트북(Zenbook UX425QA) 내장 SD 슬롯에서 `/dev/mmcblk0` (29.7G)로 인식됨. 메인 SSD는 `/dev/nvme0n1` (절대 건드리면 안 됨).

## 완료된 것
1. Docker 설치, 컨테이너 생성, apt 소스/locale/cpio 등 EOL OS 이슈 해결
2. PetaLinux 2017.4 설치 완료
3. BSP로 Zybo Z7-10 프로젝트 생성 완료 (`boatcontrol`)
4. **libgpiod 의존성 제거**: PetaLinux 2017.4 커널(~4.9)이 libgpiod 2.x가 요구하는 GPIO uAPI v2(커널 5.10+)를 지원하지 않아서, trigger 핀 GPIO 소유권을 유저스페이스(libgpiod)에서 echo_driver 커널 모듈로 이동. 수정한 파일 (전부 기기에 커밋 완료):
   - `boat/drivers/echo_driver/echo_driver.c`: trigger 핀 gpio_request/gpio_direction_output 추가, `write()` 호출을 "트리거 펄스 발생" 명령으로 처리하는 `echo_write()` 추가
   - `boat/app/src/distance_sensor_dev.cpp`: libgpiod 코드 전부 제거, `/dev/echo_driver`에 write 후 read하는 방식으로 재작성
   - `boat/app/CMakeLists.txt`: `find_library(GPIOD_LIB gpiod ...)` 및 링크 라인 제거
5. **`petalinux-build` 전체 빌드 성공** (2026-09-23). 과정에서 막혔던 이슈들과 해결법:
   - **u-boot do_fetch 실패** (Digilent u-boot 저장소가 `master` 브랜치를 없애고 연도별 `digilent_rebase_vYYYY.NN` 브랜치로 재구성함): `project-spec/meta-user/recipes-bsp/u-boot/u-boot-xlnx_%.bbappend`에서 `SRC_URI_remove`로 기존 `branch=master` git URI를 제거하고 `nobranch=1`을 추가한 버전으로 교체.
     - ⚠️ 주의: 이 bbappend를 **통째로 `SRC_URI = "..."`로 덮어쓰면 안 됨** — `project-spec/meta-plnx-generated/recipes-bsp/u-boot/`가 자동 생성한 `file://platform-auto.h`, `file://config.cfg` appends와 `meta-user`가 기본 제공하는 `file://platform-top.h` append가 같이 날아가서 `do_configure`가 `platform-auto.h`/`platform-top.h` 설치 단계에서 실패함. 반드시 `SRC_URI_remove` + `SRC_URI +=`로 git URI 항목만 정밀 교체하고, `FILESEXTRAPATHS_prepend`/`file://platform-top.h` 줄은 유지해야 함.
     - 최종 `u-boot-xlnx_%.bbappend` 내용:
       ```
       FILESEXTRAPATHS_prepend := "${THISDIR}/files:"
       SRC_URI += "file://platform-top.h"

       SRC_URI_remove = "git://github.com/digilent/u-boot-digilent.git;protocol=https;branch=master"
       SRC_URI += "git://github.com/digilent/u-boot-digilent.git;protocol=https;nobranch=1"
       ```
   - **fsbl/u-boot do_configure 타임아웃** ("timeout while establishing a connection with SDK", 로그에 "GTK+ Version Check" 에러): xsct(Eclipse 기반 HSI)가 컨테이너의 GTK3과 충돌. `sudo apt-get install -y libgtk2.0-0 libcanberra-gtk-module libxtst6` + `export SWT_GTK3=0`로 해결.
   - Digilent 데모 앱 5개(libuio, libgpio, libpwm, pwmdemo, gpioutil)는 `petalinux-config -c rootfs`에서 비활성화 (fetch 타임아웃 나던 불필요한 앱들).
6. **GPIO 핀 매핑 확정** (Digilent Zybo Z7 레퍼런스 매뉴얼 Table 16.1 기준):
   - Zybo Z7의 Pmod 중 **JF만 PS MIO에 직결** (JA~JE는 PL). JF 핀맵: JF1=MIO13, JF2=MIO10, JF3=MIO11, JF4=MIO12, JF7=MIO0, JF8=MIO9, JF9=MIO14, JF10=MIO15.
   - MIO0/MIO9는 다른 기능과 겹칠 수 있어 회피, **MIO10~15만 사용**: 모터(LEFT_FWD=10/JF2, LEFT_BWD=11/JF3, RIGHT_FWD=12/JF4, RIGHT_BWD=13/JF1) + 초음파(ECHO=14/JF9, TRIGGER=15/JF10).
   - 기존 코드가 쓰던 MIO0~5는 이 보드의 JF 커넥터로 물리적으로 접근 불가능한 핀이었음 (오류였음) — `motor_driver.c`, `echo_driver.c`의 `MIO_*` 상수를 위 값으로 수정 완료.
   - `GPIO_BASE=905`는 커널 빌드 설정(`ARCH_NR_GPIOS - ZYNQ_GPIO_NR_GPIOS`)에 따라 달라질 수 있어 확정 불가 → 두 드라이버 모두 `gpio_base`를 `module_param`으로 변경 (기본값 905, 실제 부팅 후 `cat /sys/kernel/debug/gpio`에서 `zynq_gpio` 라벨의 base 확인 후 `insmod ... gpio_base=N`으로 override).
   - HC-SR04 echo는 5V 출력, MIO는 3.3V 입력 → **레벨 시프터/전압분배 회로 필요** (미착수, 실배선 시 주의).
7. **motor_driver, echo_driver를 PetaLinux out-of-tree 커널 모듈 레시피로 등록 완료** (2026-09-24):
   - `petalinux-create -t modules -n <name> --enable` 사용 시 이름에 `_`(언더스코어)가 있으면 "이름_버전" 구분자와 충돌해서 거부됨 → 레시피 이름은 `motor-driver`, `echo-driver`(하이픈)로 생성, 실제 소스 파일명(`motor_driver.c` 등, 언더스코어)은 유지하고 `Makefile`의 `obj-m`과 `.bb`의 `SRC_URI`를 그 파일명에 맞게 직접 수정.
   - `motor-driver.bb`의 SRC_URI에 `file://motor_ioctl.h` 추가 필요 (헤더 의존성).
   - **Makefile 탭 문자 주의**: heredoc(`cat > file << EOF`)으로 Makefile을 붙여넣으면 터미널이 탭을 스페이스로 바꿔서 `Makefile:6: *** missing separator` 에러 발생. `printf '...\t...'`로 탭을 명시적으로 넣어서 재생성해야 함. `cat -A file`로 `^I`가 보이는지 확인.
   - `petalinux-build -c motor-driver`, `petalinux-build -c echo-driver` 개별 빌드 성공, 이후 전체 `petalinux-build`도 성공 (두 모듈 다 최종 이미지에 포함 확인됨).
   - `motor_driver.c`에서 `#include "motor_ioctl.h"`를 파일 최상단으로 옮긴 부분도 모듈 빌드가 문제없이 성공해서 `_IOW`/`_IO` 매크로 이슈 없음 확인됨.
8. **`petalinux-build -x populate_sdk` 실패 → 우회 성공** (2026-09-24):
   - 에러: `packagegroup-cross-canadian-plnx_arm not found in the feeds` — SDK 설치용 메타패키지의 RPM이 생성 안 됨.
   - 근본 원인 추적: `do_package_write_rpm` 로그에 `NOTE: Not creating empty RPM package for packagegroup-cross-canadian-plnx_arm` — 이 레시피는 RDEPENDS만 있고 실제 파일이 없는 순수 메타패키지라 `ALLOW_EMPTY`가 필요한데, `packagegroup.bbclass`가 자동으로 설정해주는 `ALLOW_EMPTY_<pkg>`도, 수동으로 bbappend에 추가한 `ALLOW_EMPTY_<pkg>` override도 **실제 RPM 생성 시점에 전혀 반영되지 않는** PetaLinux 2017.4의 `package_rpm.bbclass` 버그로 확인됨 (파싱 시점엔 변수값이 정확히 "1"로 확인되는데도 런타임에 무시됨). `do_install[noexec]` 해제 + placeholder 파일 강제 삽입 시도도 실패 (bbappend에서 태스크 플래그 override 자체가 안 먹힘). **이 경로는 더 깊이 파려면 `package_rpm.bbclass` 자체를 소스 레벨로 패치해야 해서 포기.**
   - **우회 방법**: `-x populate_sdk`가 패키징하려던 건 결국 크로스 툴체인인데, **그 툴체인이 이미 컨테이너 PATH에 통째로 존재함** — PetaLinux 설치 시 같이 깔린 `/workspace/tools/opt/pkg/petalinux/tools/linux-i386/gcc-arm-linux-gnueabi/bin/arm-linux-gnueabihf-g++` (Linaro GCC 6.2.1, PetaLinux가 커널/모듈 빌드에 내부적으로 쓰는 바로 그 툴체인). 그리고 완전한 타겟 sysroot(정확한 glibc 2.23, 헤더, `libstdc++.so.6.0.22` 등)도 `build/tmp/sysroots/plnx_arm`에 이미 만들어져 있음. **SDK를 설치할 필요 없이 이 둘을 직접 가리켜서 크로스컴파일하면 됨.**
   - ⚠️ **호스트에 apt로 설치한 최신 크로스컴파일러(`g++-arm-linux-gnueabihf`, Ubuntu 25.x 패키지, GCC 15)는 쓰면 안 됨**: `--sysroot`를 지정해도 이 컴파일러는 자체 내장 sysroot(`/usr/arm-linux-gnueabihf`, 최신 glibc)의 crt/libc를 우선 사용하고(`-B<sysroot>/usr/lib -B<sysroot>/lib`로 강제 가능은 함), 무엇보다 **GCC 15가 기본 제공하는 정적 `libstdc++.a` 자체가 최신 glibc 심볼(`__isoc23_strtol`, `__setsockopt64`, `__ioctl_time64` 등, glibc 2.34+)을 하드코딩하고 있어서** 타겟(glibc 2.23)과 근본적으로 ABI 불일치 — 링크 단계에서 undefined reference 에러 발생. 반드시 **타겟과 동시대인 컨테이너 내장 GCC 6.2.1**을 써야 함.
   - **최종 작동 확인된 빌드 방법** (컨테이너 안, 앱 소스는 호스트에서 `~/petalinux-work/app-build/`로 복사해서 컨테이너에 노출):
     ```bash
     SYSROOT=/workspace/boatcontrol/build/tmp/sysroots/plnx_arm
     arm-linux-gnueabihf-g++ -std=c++17 -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard \
       --sysroot=$SYSROOT -B$SYSROOT/usr/lib -B$SYSROOT/lib \
       -I<inc dirs...> \
       <소스파일...> \
       -o boat_bin
     ```
   - 결과 바이너리 검증: 요구 `GLIBC_2.4` (타겟 2.23보다 훨씬 낮음, 안전), 요구 `GLIBCXX_3.4.21` (타겟 sysroot의 `libstdc++.so.6.0.22`가 제공, 안전). `boat` 앱(USE_HW_MOTOR=ON, USE_HW_DISTANCE=ON, 실물 하드웨어 버전) 크로스컴파일 성공.
   - **미완료**: 이걸 CMake 기반으로 재현 가능하게 정리하는 작업 (지금은 g++ 직접 호출로만 검증함). 원하면 `cmake/` 밑에 이 컨테이너 툴체인을 가리키는 `.cmake` 파일을 만들어서 `cmake --build`로 재현 가능하게 만들 수 있음.
9. **부팅 이미지 생성 완료** (2026-09-25):
   - `petalinux-package --boot --fsbl images/linux/zynq_fsbl.elf --fpga images/linux/system_wrapper.bit --u-boot images/linux/u-boot.elf --force` 로 `BOOT.BIN` 생성 성공.
     - ⚠️ 주의: `--fsbl`, `--fpga`, `--u-boot` 뒤에 **파일 경로를 명시적으로 지정해야 함**. 인자 없이 플래그만 나열하면(`--fsbl --fpga --u-boot`) `--fsbl`이 다음 토큰 `--fpga`를 자기 인자로 먹어버려서 `readlink: unrecognized option '--fpga'` 에러 발생.
   - `petalinux-package --wic`는 **PetaLinux 2017.4에 존재하지 않는 옵션** (`--boot|--bsp|--image|--prebuilt`만 지원, wic 자동 이미지 생성은 후기 버전 기능) → SD카드 수동 파티셔닝으로 진행.
   - **SD카드 수동 구성 완료**: 호스트(컨테이너 밖)에서 `/dev/mmcblk0`(29.7G, 노트북 내장 SD슬롯)에 대해:
     - `parted`로 파티션 2개 생성: p1 FAT32 4MiB~204MiB(BOOT, boot flag on), p2 ext4 204MiB~100%(rootfs)
     - `mkfs.vfat -F 32 -n BOOT /dev/mmcblk0p1`, `mkfs.ext4 -L rootfs /dev/mmcblk0p2`
     - BOOT 파티션에 `BOOT.BIN`, `image.ub` 복사
     - rootfs 파티션에 `rootfs.tar.gz`를 `sudo tar xzf ... --numeric-owner`로 압축 해제 (uid/gid 보존을 위해 `--numeric-owner` 필수)
     - 마운트 재확인으로 양쪽 파티션 내용 검증 완료 (BOOT.BIN/image.ub 존재, rootfs에 bin/etc/lib/usr/var 등 정상 확인)
   - **호스트 파일 경로 참고**: 컨테이너 `/workspace/boatcontrol/images/linux/` = 호스트 `~/petalinux-work/boatcontrol/images/linux/` (같은 마운트라 컨테이너에서 만든 빌드 산출물이 호스트에서 바로 보임, 복사 불필요).

## 다음 단계 (미완료)
- **SD카드를 Zybo Z7-10에 물리적으로 장착** (사용자가 다음에 진행하기로 함, 2026-09-25 기준 대기 중)
  - 부팅 모드 점퍼(JP5)를 SD로 설정
  - PROG/UART 마이크로USB로 PC와 연결 (전원 겸용, JP7 전원 점퍼가 USB인지 확인)
  - `dmesg`/`ls /dev/ttyUSB*`로 시리얼 콘솔 장치 확인, `screen /dev/ttyUSBx 115200`으로 부팅 로그 확인
- 부팅 후 실측: `zynq_gpio` gpiochip의 실제 base 번호 확인 (`cat /sys/kernel/debug/gpio`) → `insmod motor-driver.ko gpio_base=N`, `insmod echo-driver.ko gpio_base=N`으로 반영, `dmesg` 확인
- `boat_bin`(크로스컴파일 완료됨, 위 8번 참고)을 보드로 전송(scp 또는 SD카드 재사용)하여 실행, `/dev/motor_driver`·`/dev/echo_driver` 통신 확인
- boat 앱 CMake 빌드를 컨테이너 내장 툴체인 기준으로 재현 가능하게 정리 (선택)
- HC-SR04 echo 라인 레벨 시프터/전압분배 회로 준비 (미착수, 실배선 전 필수 — echo 5V vs MIO 3.3V)