# Zybo Z7-10 PetaLinux — SDK/크로스컴파일 문제 해결 정리

마지막 업데이트: 2026-09-24

## 환경
- 보드: Zybo Z7-10 (Zynq-7010)
- PetaLinux 2017.4 (Digilent 공식 BSP 기준 고정)
- 호스트: 네이티브 Ubuntu 26.04
- 작업 환경: Docker 컨테이너 (Ubuntu 16.04 base, glibc 2.23)
  - 마운트: 호스트 `~/petalinux-work` ↔ 컨테이너 `/workspace`
  - 프로젝트 경로: `/workspace/boatcontrol`
  - 주의: `BoatControl` 앱 저장소(`~/YGC/soloProject/BoatControl`)는 이 마운트와 별개라 컨테이너에서 안 보임 → 필요한 소스는 호스트에서 `~/petalinux-work/` 밑으로 복사해야 컨테이너에서 접근 가능

---

## 1. 문제: `petalinux-build -x populate_sdk` 실패

### 증상
```
ERROR: ... do_populate_sdk: packagegroup-cross-canadian-plnx_arm not found in the feeds
(x86_64-nativesdk noarch any all) in .../build/tmp/deploy/rpm.
```

### 원인 추적 과정
1. `-x sdk`는 잘못된 태스크명 (`do_sdk` 없음) → `-x populate_sdk`가 맞는 명령.
2. sstate 캐시 문제로 의심 → `cleansstate` 후 재빌드해도 동일 에러.
3. RPM 피드 인덱스 문제로 의심 → `package-index` 재생성해도 동일 에러.
4. 실제 파일시스템 확인: 개별 툴체인 컴포넌트 RPM(gcc-cross-canadian-arm 등)은 존재하지만, **메타패키지 자체의 RPM만 어디에도 없음**.
5. 빌드 로그(`log.do_package_write_rpm`) 직접 확인 → 결정적 단서:
   ```
   NOTE: Not creating empty RPM package for packagegroup-cross-canadian-plnx_arm
   ```
   이 레시피는 RDEPENDS만 있고 실제 파일이 없는 순수 메타패키지라, `ALLOW_EMPTY`가 "1"이어야 빈 RPM이라도 생성됨.

### 근본 원인 (PetaLinux 2017.4 버그로 결론)
- `packagegroup.bbclass`는 원래 모든 PACKAGES에 대해 `ALLOW_EMPTY_<pkg>`를 자동으로 "1"로 설정함 (표준 동작) — 그런데도 안 먹힘.
- 수동으로 bbappend에 `ALLOW_EMPTY_packagegroup-cross-canadian-plnx_arm = "1"`을 추가해서 **파싱 시점에 값이 정확히 "1"로 들어가는 것까지 확인**(진단용 `bb.fatal()`로 직접 출력해서 검증)했지만, 실제 `do_package_write_rpm` 실행 시점엔 여전히 무시됨.
- `do_install[noexec] = "1"`(packagegroup.bbclass가 걸어둔 플래그)을 bbappend에서 해제(`""`)하고 placeholder 파일을 강제로 심어서 "비어있지 않게" 만드는 것도 시도했으나, `do_install` 태스크 자체가 끝내 실행되지 않음 (로그 파일조차 생성 안 됨).
- 결론: `package_rpm.bbclass`의 override-folding 로직이 이 특정 케이스(nativesdk/cross-canadian 메타패키지)에서 제대로 동작하지 않는 버그성 동작. 더 깊이 고치려면 `package_rpm.bbclass` 자체를 소스 레벨로 패치해야 하는 수준 → **여기서 이 경로는 포기**.

---

## 2. 우회 방법: SDK 설치 없이 직접 크로스컴파일

### 핵심 발견
`populate_sdk`가 패키징하려던 건 결국 "크로스 툴체인 + 타겟 sysroot"인데, **이 둘이 이미 컨테이너 안에 완성된 형태로 존재**했음:

- **크로스 컴파일러**: `/workspace/tools/opt/pkg/petalinux/tools/linux-i386/gcc-arm-linux-gnueabi/bin/arm-linux-gnueabihf-g++`
  (Linaro GCC 6.2.1 — PetaLinux가 커널/모듈 빌드에 실제로 쓰는 바로 그 툴체인, `settings.sh` 소싱 시 PATH에 자동으로 잡힘)
- **타겟 sysroot**: `build/tmp/sysroots/plnx_arm`
  (정확한 타겟용 glibc 2.23, 헤더, `libstdc++.so.6.0.22` 등 이미 빌드되어 있음)

이 둘을 직접 가리켜서 컴파일하면 SDK 설치 과정 자체가 필요 없음.

### ⚠️ 시행착오: 호스트에 apt로 설치한 최신 컴파일러는 쓰면 안 됨
처음엔 호스트(Ubuntu 26.04)에 `apt install g++-arm-linux-gnueabihf`로 최신 GCC 15를 설치해서 시도했으나:
- `--sysroot`를 지정해도 이 컴파일러는 자체 내장 sysroot(`/usr/arm-linux-gnueabihf`, 최신 glibc)의 crt/libc를 우선 사용함 → `-B<sysroot>/usr/lib -B<sysroot>/lib`로 강제 우선순위 지정하면 이 부분은 해결 가능.
- 하지만 더 근본적인 문제: **GCC 15가 기본 제공하는 정적 `libstdc++.a` 자체가 최신 glibc 전용 심볼**(`__isoc23_strtol`, `__setsockopt64`, `__ioctl_time64` 등, glibc 2.34+에만 존재)을 이미 컴파일된 형태로 내부에 갖고 있어서, 타겟의 구형 glibc(2.23)와 링크 단계에서부터 `undefined reference` 에러 발생. 이건 컴파일 플래그로 우회 불가능한 하드웨어적 ABI 불일치.
- 결론: **반드시 타겟과 동시대(2016~2017년)에 만들어진 컴파일러**를 써야 함 → 컨테이너 내장 GCC 6.2.1로 전환하니 바로 해결됨.

### 최종 작동 확인된 빌드 명령 (컨테이너 안에서)
```bash
# 앱 소스를 호스트에서 컨테이너가 보는 경로로 복사 (한 번은 호스트 터미널에서)
# cp -r ~/YGC/soloProject/BoatControl/BoatControl_cpp/boat ~/petalinux-work/app-build/
# cp -r ~/YGC/soloProject/BoatControl/BoatControl_cpp/common ~/petalinux-work/app-build/

cd /workspace/app-build
SYSROOT=/workspace/boatcontrol/build/tmp/sysroots/plnx_arm

arm-linux-gnueabihf-g++ -std=c++17 -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard \
  --sysroot=$SYSROOT -B$SYSROOT/usr/lib -B$SYSROOT/lib \
  -Iboat/app/inc -Icommon -Iboat/drivers/motor_driver \
  boat/app/src/main.cpp boat/app/src/udp_receiver.cpp \
  boat/app/src/motor_controller_dev.cpp boat/app/src/distance_sensor_dev.cpp \
  -o boat_bin
```

### 결과 바이너리 검증
```bash
file boat_bin
# ELF 32-bit LSB executable, ARM, EABI5, dynamically linked, interpreter /lib/ld-linux-armhf.so.3

arm-linux-gnueabihf-objdump -T boat_bin | grep -oP 'GLIBC_[0-9.]+' | sort -Vu | tail -5
# 최대 GLIBC_2.4 요구 (타겟 2.23보다 훨씬 낮음 → 안전)

arm-linux-gnueabihf-objdump -T boat_bin | grep -oP 'GLIBCXX_[0-9.]+' | sort -Vu | tail -5
# 최대 GLIBCXX_3.4.21 요구 (타겟 sysroot의 libstdc++.so.6.0.22가 제공 → 안전)
```

USE_HW_MOTOR=ON, USE_HW_DISTANCE=ON (실물 하드웨어 버전)으로 크로스컴파일 성공.

---

## 3. 남은 일

- [ ] g++ 직접 호출 빌드를 CMake 기반으로 재현 가능하게 정리 (toolchain 파일 작성, 선택 사항)
- [ ] `petalinux-package --boot`, `petalinux-package --wic`로 SD카드 이미지 생성
- [ ] SD카드 굽기 → 실제 Zybo Z7-10 보드 부팅 확인
- [ ] 부팅 후 `cat /sys/kernel/debug/gpio`로 `zynq_gpio`의 실제 gpio base 확인 → `insmod motor-driver.ko gpio_base=N` / `insmod echo-driver.ko gpio_base=N`으로 반영
- [ ] HC-SR04 echo 라인 5V→3.3V 레벨 시프터/전압분배 회로 준비 (미착수)

---

## 부록: 이번 세션에서 함께 해결된 것들 (요약)

- **u-boot do_fetch 실패**: Digilent u-boot 저장소 브랜치 구조 변경 대응 (`SRC_URI_remove` + `SRC_URI +=`로 git URI만 정밀 교체, 전체 덮어쓰기 금지)
- **fsbl/u-boot do_configure 타임아웃**: xsct/GTK3 충돌 → `libgtk2.0-0` 등 설치 + `SWT_GTK3=0`
- **GPIO 핀 매핑 오류 수정**: 기존 코드가 쓰던 MIO0~5는 Zybo Z7의 JF 커넥터로 물리적 접근 불가 → MIO10~15로 재배정, `GPIO_BASE`를 컴파일 타임 상수에서 `module_param`으로 변경
- **motor_driver/echo_driver 커널 모듈 레시피 등록**: 언더스코어 네이밍 제약(하이픈으로 우회), Makefile 탭 문자 손상(printf로 재생성) 등 해결, 전체 이미지 빌드 성공
