# BoatControl 빌드 & 실행 가이드 (Zybo Z7-10)

마지막 업데이트: 2026-09-26

이 문서는 "작업 기록"이 아니라 **매번 빌드/실행할 때 그대로 따라 하는 실행 가이드**입니다.
세션별 작업 히스토리는 `report_YYYYMMDD.md` 파일들을 참고하세요.

---

## 0. 전체 구조

- **`vision`** — 노트북(호스트)에서 실행. 카메라 캡처 → (스텁 또는 ONNX) 객체 탐지 → `bcc::DetectionPacket`을 UDP로 보드에 전송.
- **`boat`** — Zybo Z7-10 보드(PetaLinux)에서 실행. 초음파 센서(`echo_driver`)로 장애물 체크 → UDP로 받은 탐지 결과 기반 모터(`motor_driver`) 제어.
- 통신: UDP, 기본 포트 **5555** (`common/detection_packet.h`의 `kDefaultBoatPort`), 패킷 크기 고정 204바이트, magic `0xD5C1`.

---

## 1. 매 세션 사전 준비 (컨테이너)

```bash
# 컨테이너 시작 (꺼져있을 때만)
docker start petalinux-2017.4

# 인터랙티브 접속
docker exec -u petalinux -it petalinux-2017.4 bash
```

컨테이너 안에서 매번:
```bash
source /workspace/tools/opt/pkg/petalinux/settings.sh
export SWT_GTK3=0
```

---

## 2. `boat` 앱 빌드 (보드용 크로스컴파일)

앱 소스 저장소(`~/YGC/soloProject/BoatControl`)는 컨테이너 마운트(`~/petalinux-work` ↔ `/workspace`)와 **다른 경로**라 컨테이너에서 안 보입니다. 소스를 고칠 때마다 아래처럼 동기화해야 합니다 (호스트 터미널, 컨테이너 밖):

```bash
# 최초 1회 (전체 복사)
cp -r ~/YGC/soloProject/BoatControl/BoatControl_cpp/boat ~/petalinux-work/app-build/
cp -r ~/YGC/soloProject/BoatControl/BoatControl_cpp/common ~/petalinux-work/app-build/

# 이후 특정 파일만 수정했을 때 (예: main.cpp)
cp ~/YGC/soloProject/BoatControl/BoatControl_cpp/boat/app/src/main.cpp \
   ~/petalinux-work/app-build/boat/app/src/main.cpp
```

컨테이너 안에서 빌드 (실물 하드웨어 버전: `USE_HW_MOTOR=ON`, `USE_HW_DISTANCE=ON`에 해당하는 소스 조합):

```bash
cd /workspace/app-build
SYSROOT=/workspace/boatcontrol/build/tmp/sysroots/plnx_arm

arm-linux-gnueabihf-g++ -std=c++17 -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard \
  --sysroot=$SYSROOT -B$SYSROOT/usr/lib -B$SYSROOT/lib \
  -Iboat/app/inc -Icommon -Iboat/drivers/motor_driver \
  boat/app/src/main.cpp boat/app/src/udp_receiver.cpp \
  boat/app/src/motor_controller_dev.cpp boat/app/src/distance_sensor_dev.cpp \
  -o boat_bin
```

⚠️ **반드시 이 컨테이너 내장 GCC 6.2.1을 써야 함** — 호스트에 apt로 설치한 최신 `g++-arm-linux-gnueabihf`(GCC 15)는 타겟 glibc(2.23)와 ABI가 안 맞아 링크 에러 남. 자세한 이유는 `petalinux-sdk-troubleshooting-report.md` 참고.

결과물: `/workspace/app-build/boat_bin` = 호스트 `~/petalinux-work/app-build/boat_bin` (같은 마운트라 바로 접근 가능, 컨테이너 밖으로 복사 불필요).

---

## 3. 보드-노트북 네트워크 연결 (매 세션)

USB-이더넷 어댑터로 노트북 ↔ 보드 이더넷 포트를 다이렉트 연결 (공유기 없이 DHCP 서버가 없으므로 양쪽에 고정 IP 필요).

**노트북 쪽** (인터페이스 이름은 `ip addr`로 매번 확인, 보통 `enx...`):
```bash
sudo ip addr add 192.168.10.2/24 dev enx5414a7e48009
sudo ip link set enx5414a7e48009 up
```

**보드 쪽** (시리얼 콘솔 또는 이전 세션에서 이미 SSH 연결이 살아있다면 그걸로):
```bash
ifconfig eth0 192.168.10.10 netmask 255.255.255.0 up
```

**SSH 접속** (보드의 Dropbear가 구버전이라 최신 OpenSSH 클라이언트와 호환 옵션 필요):
```bash
ssh -o HostKeyAlgorithms=+ssh-rsa -o PubkeyAcceptedAlgorithms=+ssh-rsa root@192.168.10.10
```

**파일 전송** (`scp`도 동일한 이유로 옵션 필요 + Dropbear에 SFTP 서버가 없어서 `-O`로 레거시 프로토콜 강제):
```bash
scp -O -o HostKeyAlgorithms=+ssh-rsa -o PubkeyAcceptedAlgorithms=+ssh-rsa \
  ~/petalinux-work/app-build/boat_bin root@192.168.10.10:/root/
```

---

## 4. 보드 부팅 후 커널 모듈 로드 (매 부팅마다)

```bash
insmod /lib/modules/4.9.0-xilinx-v2017.4/extra/motor_driver.ko gpio_base=905
insmod /lib/modules/4.9.0-xilinx-v2017.4/extra/echo_driver.ko gpio_base=905
```

`905`는 `zynq_gpio`의 실측 base 번호 (아래 명령으로 재확인 가능, debugfs가 꺼져있는 커널이라 `/sys/kernel/debug/gpio` 대신 이 경로 사용):
```bash
cat /sys/class/gpio/gpiochip*/label
cat /sys/class/gpio/gpiochip*/base
```
`zynq_gpio` 라벨 옆의 base 값이 바뀌면 위 `insmod`의 `gpio_base=`도 그 값으로 바꿔야 함.

확인:
```bash
lsmod
dmesg | tail -20   # "motor_driver: 초기화 완료", "echo_driver: 초기화 완료 (trigger=..., echo=...)" 확인
ls -l /dev/motor_driver /dev/echo_driver
```

---

## 5. `boat` 실행 (보드)

```bash
chmod +x /root/boat_bin
./boat_bin
```

정상 실행되면 프롬프트로 안 돌아오고 대기 상태가 됩니다. **콘솔에 아무것도 안 찍혀도 정상입니다** (기본 코드는 조용히 동작함 — 디버그 로그를 넣은 버전이면 UDP 패킷 수신 시 `[UDP] packet received: ...`가 찍힘). 종료는 `Ctrl+C`.

옵션: `--port <포트번호>` (기본 5555)

---

## 6. `vision` 빌드 & 실행 (노트북, 네이티브 — 크로스컴파일 아님)

```bash
cd ~/YGC/soloProject/BoatControl/BoatControl_cpp
mkdir -p build-vision && cd build-vision
cmake .. -DBUILD_VISION=ON -DBUILD_BOAT=OFF
cmake --build . --target vision -j$(nproc)
```

- OpenCV 필요: `pkg-config --modversion opencv4`로 설치 확인, 없으면 `sudo apt install -y libopencv-dev`
- 기본은 스텁 디텍터(`USE_ORT=OFF`, 항상 탐지 0개 반환) — 실제 YOLO 추론 쓰려면 `-DUSE_ORT=ON -DONNXRUNTIME_ROOT=<경로>`로 빌드 (모델: `model/yolov10n.onnx`)

실행:
```bash
./vision/vision --host 192.168.10.10 --port 5555 --camera 0
```
`--camera`는 카메라 인덱스 (보통 내장캠 0). 종료는 영상 창에서 `q`/`ESC` 또는 창 닫기.

---

## 7. 전체 구동 순서 요약 (체크리스트)

1. 보드에 SD카드 장착, 점퍼 확인, USB(전원/UART) + 이더넷 연결, 전원 ON
2. 보드-노트북 이더넷 고정 IP 설정 (섹션 3)
3. SSH 접속 (섹션 3)
4. `insmod` 두 모듈 (섹션 4)
5. 보드에서 `./boat_bin` 실행 (섹션 5)
6. 노트북에서 `./vision/vision --host <보드IP> --port 5555 --camera 0` 실행 (섹션 6)
7. 보드 콘솔에서 정상 수신 확인 (디버그 로그 버전이면 `[UDP] packet received...` 로그로)

---

## 트러블슈팅 메모

- **시리얼 콘솔(`screen /dev/ttyUSB1 115200`)이 답답하고 느림** → FTDI 레이턴시 타이머 문제일 수 있음: `echo 1 | sudo tee /sys/bus/usb-serial/devices/ttyUSB1/latency_timer`. 근본적으로는 SSH(섹션 3)로 넘어가는 게 훨씬 쾌적함.
- **터미널이 갑자기 먹통** → `Ctrl+S`(XOFF) 실수로 눌렸을 가능성 큼 → `Ctrl+Q`로 해제.
- **`ssh`/`scp`가 "no matching host key type" 에러** → 위 섹션 3의 `-o HostKeyAlgorithms=+ssh-rsa -o PubkeyAcceptedAlgorithms=+ssh-rsa` 옵션 필수 (보드의 구형 Dropbear가 `ssh-rsa`만 지원, 최신 OpenSSH 클라이언트는 기본적으로 이걸 막아둠).
- **`scp`가 안 됨(sftp 관련)** → Dropbear에 `sftp-server`가 없어서 최신 scp의 기본 SFTP 프로토콜이 실패함 → `-O` 옵션으로 레거시 scp 프로토콜 사용.
- **`cat /sys/kernel/debug/gpio`가 "No such file or directory"** → 이 커널은 `CONFIG_DEBUG_FS`가 꺼져있음 → `/sys/class/gpio/gpiochip*/{label,base}`로 대체.
- **`boat_bin` 실행 후 아무 반응 없어 보임** → 버그 아님. 콘솔 출력이 원래 없는 조용한 제어 루프임 (초음파 40ms 타임아웃 + UDP 200ms 타임아웃 주기로 계속 돌면서, 패킷 없으면 랜덤 방향 모터 명령을 조용히 내림). 동작 확인하려면 위 main.cpp에 추가한 디버그 로그 버전을 쓰거나 `Ctrl+C` 후 `dmesg`로 open/release 로그 확인.
- **BusyBox `nc`로 UDP 리스너 안 뜸** → 이 rootfs의 BusyBox `nc`는 최소 옵션판이라 `-l`/`-u`/`-p` 미지원. `tcpdump`/`python3`도 rootfs에 없음. 필요하면 작은 C 프로그램을 크로스컴파일해서 올리는 방법 사용 (섹션 2와 동일한 빌드 방식).

---

## 참고 상수/경로

- UDP 포트: `5555` (`common/detection_packet.h::kDefaultBoatPort`)
- GPIO base: `905` (zynq_gpio, 부팅마다 `/sys/class/gpio/gpiochip*/base`로 재확인 권장)
- GPIO 핀맵: 모터 LEFT_FWD=MIO10(JF2), LEFT_BWD=MIO11(JF3), RIGHT_FWD=MIO12(JF4), RIGHT_BWD=MIO13(JF1) / 초음파 ECHO=MIO14(JF9), TRIGGER=MIO15(JF10)
- 커널 모듈 경로(보드): `/lib/modules/4.9.0-xilinx-v2017.4/extra/{motor_driver,echo_driver}.ko`
- 디바이스 노드: `/dev/motor_driver`, `/dev/echo_driver`
- 시리얼 콘솔: `/dev/ttyUSB1` @ 115200 (FT2232H, `ttyUSB0`은 JTAG)
