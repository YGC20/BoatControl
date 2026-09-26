# PetaLinux 개발 환경 구축 진행 상황 (Zybo Z7)

마지막 업데이트: 2026-09-23

## 결정된 방향
- Digilent 공식 BSP 기준 → **PetaLinux 2017.4** 고정 사용 (Zybo Z7-10/Z7-20 최신 공식 BSP가 모두 이 버전 타깃)
- 호스트: 네이티브 Ubuntu 26.04 PC (용량 278GB 여유)
- PetaLinux 2017.4는 Ubuntu 16.04 전용이라 호스트에 직접 설치 불가 → **Docker (Ubuntu 16.04 base) 컨테이너**로 격리해서 진행 중

## 완료된 것
1. Docker Engine 호스트에 설치 완료 (v29.8.1)
2. 컨테이너 생성 완료:
   ```
   docker run -it --name petalinux-2017.4 -v ~/petalinux-work:/workspace ubuntu:16.04 bash
   ```
   (재진입: `docker start -ai petalinux-2017.4`)
3. apt 소스 수정: `old-releases.ubuntu.com`이 xenial(16.04)을 더 이상 서빙하지 않아서(완전히 제거됨), **`mirror.seas.harvard.edu`**로 sources.list 교체해서 사용. 단, 이 이미지엔 `apt-transport-https`가 없어서 **http(https 아님)**로 접속해야 함.
4. 빌드 의존 패키지 설치 완료 (i386 아키텍처 활성화 포함: `dpkg --add-architecture i386`)
5. `/bin/sh`를 dash → bash로 전환 완료
6. GNU Make 3.81을 `/workspace/tools`에 로컬 빌드 완료, PATH에 추가
7. PetaLinux 2017.4 인스톨러(`petalinux-v2017.4-final-installer.run`) 다운로드 및 설치 완료
   - 설치 경로: `/workspace/tools/opt/pkg/petalinux`
   - 사용 전 매번: `source /workspace/tools/opt/pkg/petalinux/settings.sh`
   - **root로 설치 불가** → 일반 사용자(`petalinux`, sudo 그룹) 생성해서 그 계정으로 설치함
   - 설치 중 만난 이슈와 해결:
     - `en_US.UTF-8` locale 없음 → `apt-get install locales && locale-gen en_US.UTF-8 && update-locale LANG=en_US.UTF-8`
     - `cpio` 없음 → `apt-get install cpio`
     - `petalinux-env-check` 스크립트의 `unary operator expected` 경고들은 스크립트 자체 버그로, 설치 진행에는 지장 없음 (무시 가능)
   - zynqMP/zynq/microblaze용 Yocto SDK 전부 정상 설치 확인됨

## 다음 단계 (미완료)
- **보드 모델 확정 필요**: Zybo Z7-10 vs Z7-20 (BSP 리포지토리가 다름: `Digilent/Petalinux-Zybo-Z7-10` vs `Digilent/Petalinux-Zybo-Z7-20`)
- BSP 다운로드 → `petalinux-create -t project -s <bsp>`로 프로젝트 생성
- `petalinux-config -c rootfs`에서 libgpiod 패키지(`libgpiod`, `libgpiod-dev`, `libgpiod-tools`) 추가
- Device tree(`system-user.dtsi`)에 GPIO 핀 매핑 (motor_driver, echo_driver용) 추가 — Vivado 블록 디자인과 대조 필요
- `petalinux-build -x sdk`로 크로스컴파일용 SDK 추출 → boat 앱(C++/CMake) 및 커널 모듈(motor_driver, echo_driver) 크로스컴파일에 사용
- `petalinux-create -t modules -n motor_driver --enable` / `echo_driver`로 out-of-tree 커널 모듈 레시피 생성 권장 (KDIR 수동 관리 불필요)
- `petalinux-package --boot`, `petalinux-package --wic`로 이미지 생성 → SD카드 굽기 → 보드 부팅 확인


# PetaLinux 개발 환경 구축 진행 상황 (Zybo Z7-10)

마지막 업데이트: 2026-09-23

## 결정된 방향
- 보드: **Zybo Z7-10** (Zynq-7010)
- Digilent 공식 BSP 기준 → **PetaLinux 2017.4** 고정 사용
- 호스트: 네이티브 Ubuntu 26.04 PC (용량 278GB 여유)
- Docker (Ubuntu 16.04 base) 컨테이너로 격리해서 진행 중
  - 컨테이너: `docker start -ai petalinux-2017.4` (재진입)
  - 마운트: 호스트 `~/petalinux-work` ↔ 컨테이너 `/workspace`
  - PetaLinux 설치 경로: `/workspace/tools/opt/pkg/petalinux`
  - 매 세션 시작 시: `source /workspace/tools/opt/pkg/petalinux/settings.sh`
  - 일반 사용자 `petalinux` 계정으로 작업 (root 설치 불가 이슈로 생성)

## 완료된 것
1. Docker 설치, 컨테이너 생성
2. apt 소스: `mirror.seas.harvard.edu` (http, xenial) 사용 — old-releases.ubuntu.com이 xenial을 완전히 제거해서 대체 필요했음
3. 빌드 의존 패키지 설치 (i386 아키텍처 포함)
4. `/bin/sh` dash → bash 전환
5. GNU Make 3.81 로컬 빌드
6. PetaLinux 2017.4 설치 완료 (locale, cpio 이슈 해결하며 진행)
7. **BSP 다운로드 + 프로젝트 생성 완료**:
   - BSP: `https://github.com/Digilent/Petalinux-Zybo-Z7-10/releases/download/v2017.4-1/Petalinux-Zybo-Z7-10-2017.4-1.bsp`
   - `petalinux-create -t project -s <bsp> -n boatcontrol` → `/workspace/boatcontrol`에 프로젝트 생성됨

## 다음 단계 (미완료)
- `petalinux-config -c rootfs`에서 libgpiod 패키지(`libgpiod`, `libgpiod-dev`, `libgpiod-tools`) 추가
- `petalinux-config -c kernel`에서 GPIO 관련 드라이버 옵션 확인
- Device tree(`project-spec/meta-user/recipes-bsp/device-tree/files/system-user.dtsi`)에 GPIO 핀 매핑 (motor_driver, echo_driver용) 추가 — Vivado 블록 디자인과 대조 필요
- `petalinux-build` 첫 빌드 → `petalinux-build -x sdk`로 크로스컴파일용 SDK 추출
- boat 앱(C++/CMake) 및 커널 모듈(motor_driver, echo_driver) 크로스컴파일
- `petalinux-create -t modules -n motor_driver --enable` / `echo_driver`로 out-of-tree 커널 모듈 레시피 생성 권장
- `petalinux-package --boot`, `petalinux-package --wic`로 이미지 생성 → SD카드 굽기 → 보드 부팅 확인


# PetaLinux 개발 환경 구축 진행 상황 (Zybo Z7-10)

마지막 업데이트: 2026-09-23

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
  - GTK3 이슈 때문에 매 세션 `export SWT_GTK3=0` 필요 (아래 참고, ~/.bashrc에 추가 권장)

## 완료된 것
1. Docker 설치, 컨테이너 생성, apt 소스/locale/cpio 등 EOL OS 이슈 해결
2. PetaLinux 2017.4 설치 완료
3. BSP로 Zybo Z7-10 프로젝트 생성 완료 (`boatcontrol`)
4. **libgpiod 의존성 제거**: PetaLinux 2017.4 커널(~4.9)이 libgpiod 2.x가 요구하는 GPIO uAPI v2(커널 5.10+)를 지원하지 않아서, trigger 핀 GPIO 소유권을 유저스페이스(libgpiod)에서 echo_driver 커널 모듈로 이동. 수정한 파일 (전부 기기에 커밋 완료):
   - `boat/drivers/echo_driver/echo_driver.c`: trigger 핀(GPIO_BASE+5=910) gpio_request/gpio_direction_output 추가, `write()` 호출을 "트리거 펄스 발생" 명령으로 처리하는 `echo_write()` 추가
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

## 다음 단계 (미완료)
- Device tree(`project-spec/meta-user/recipes-bsp/device-tree/files/system-user.dtsi`)에 GPIO 핀 매핑 (motor_driver, echo_driver용) 확인/추가 — 현재 코드의 `GPIO_BASE=905` 가정을 Vivado 블록 디자인/보드 레퍼런스와 대조 필요
- `petalinux-create -t modules -n motor_driver --enable` / `echo_driver`로 out-of-tree 커널 모듈 레시피 생성
- boat 앱(C++/CMake) 크로스컴파일 — `petalinux-build -x sdk`로 SDK 추출 후 진행
- `petalinux-package --boot`, `petalinux-package --wic`로 이미지 생성 → SD카드 굽기 → 실제 보드 부팅 확인