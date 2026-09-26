# BoatControl

객체 인식 기술을 활용해 수면의 쓰레기를 자율적으로 수거하는 보트를 만들기 위한 프로젝트입니다.
영상 처리(웹캠 캡처 + ONNX Runtime 추론)와 하드웨어 제어(모터 · 초음파 센서)를 두 장비로 나누어, UDP 소켓으로 탐지 결과를 주고받는 구조로 개발 중입니다.

> 🚧 **진행 중인 프로젝트입니다.** 2026-09-26 기준, 실제 Zybo Z7-10 보드에서 부팅부터 커널 모듈 로드, `vision`↔`boat` UDP 통신까지 소프트웨어 스택 전체를 하드웨어에서 검증했습니다. 남은 작업은 주로 초음파 센서·모터 실배선과 커스텀 탐지 모델 적용입니다. 자세한 빌드/실행 절차는 [`document/report/boat-build-and-run-guide.md`](document/report/boat-build-and-run-guide.md), 세션별 작업 기록은 `document/report/report_YYYYMMDD.md`를 참고하세요.

## 진행 배경

친구의 졸업 작품을 돕기 위해 시작된 프로젝트로, 임베디드 보드 활용과 객체 인식 프로그래밍 학습을 주된 목표로 삼았습니다.

- 처음에는 Python으로, 라즈베리파이 한 대에서 웹캠 영상 처리(YOLO 추론)와 모터·초음파 센서 제어를 모두 담당하는 구조로 시작했습니다 (`BoatControl_py/`).
- 이후 단순 폴링 방식으로 객체를 탐지하던 기존 구조에서 나아가, C/C++로 객체 탐지 파이프라인과 하드웨어 제어를 직접 구현해보며 학습하기 위해 C++ 버전(`BoatControl_cpp/`)으로 전환했습니다. (아래 [Python → C/C++ 전환 이유](#python--cc-전환-이유) 참고)
- 전환 과정에서 실제로 쓸 수 있는 보드가 라즈베리파이가 아니라 **Zybo Z7-10(Zynq-7010, PetaLinux)**으로 정해지면서, 노트북(영상 처리)과 Zybo 보드(모터 제어)로 역할을 나누는 2-장비 구조로 다시 설계했습니다. Digilent 공식 BSP 기준으로 **PetaLinux 2017.4**를 고정 사용 중이며, vision은 Windows/Linux 양쪽에서 빌드되도록 소켓·빌드 스크립트를 맞췄습니다.

## Python → C/C++ 전환 이유

기존 Python 버전(`BoatControl_py/`)은 라즈베리파이에서 단순 폴링 방식으로 객체를 탐지하고, 그 결과에 따라 모터를 제어하는 구조였습니다. 이 프로젝트를 통해 C/C++로 객체 탐지와 하드웨어 제어(소켓 통신, GPIO 등)를 직접 구현해보며 배우는 것을 목표로 C++ 버전(`BoatControl_cpp/`)으로 전환을 진행했습니다. Python 버전은 이전 구현 기록으로 저장소에 남겨두었고, 실제 개발은 C++ 버전을 중심으로 진행하고 있습니다.

## boat 하드웨어 제어 방식: 커널 캐릭터 디바이스 드라이버

처음에는 PS MIO GPIO를 `libgpiod`로 유저스페이스에서 직접 제어하는 방식(모터는 ioctl 드라이버, 초음파 trigger만 유저스페이스 `libgpiod`)으로 boat를 설계했습니다. 그런데 실제로 PetaLinux 2017.4가 쓰는 커널(~4.9)은 `libgpiod` 2.x가 요구하는 GPIO uAPI v2(커널 5.10+)를 지원하지 않아서 이 하이브리드 구조가 동작하지 않았습니다. 그래서 **trigger 핀도 `echo_driver` 커널 모듈이 직접 소유**하도록 옮겼습니다 — 유저스페이스에서 `/dev/echo_driver`에 `write()`를 호출하면 그 자체를 "trigger 펄스 발생" 명령으로 처리합니다. 결과적으로 모터·초음파 GPIO 전부 유저스페이스 라이브러리 없이 두 커널 캐릭터 디바이스 드라이버(`motor_driver`, `echo_driver`)가 전담하는 구조가 됐습니다.

## 주요 기능

### 구현 완료 (소프트웨어, 실물 하드웨어 검증 완료 — 2026-09-26)

- **실시간 객체 탐지 (vision, C++)** — 웹캠 프레임을 캡처해 ONNX Runtime으로 YOLOv10n 모델 추론을 수행하고, 탐지 결과를 매 프레임 UDP로 전송합니다. ONNX Runtime 없이 빌드하면(`USE_ORT=OFF`, 기본값) stub 탐지기로 동작하며(항상 탐지 0개), 실제 카메라로 캡처해 UDP 송신까지 하는 파이프라인 자체는 이걸로도 검증됩니다. 커스텀 학습 모델은 아직 없어 사전학습 COCO 모델(또는 stub)로 파이프라인만 검증한 상태입니다. `--host`/`--port`/`--camera` CLI 인자 모두 지원합니다.
- **vision ↔ boat UDP 통신 프로토콜** — 탐지 결과(바운딩 박스, 클래스, confidence)를 고정 크기 구조체(`DetectionPacket`, magic `0xD5C1`, 204바이트, 기본 포트 5555)로 패킹해 한 데이터그램 = 한 프레임으로 송수신합니다. 재전송·순서 보장이 필요 없는 "덮어쓰기" 성격의 제어 데이터라 TCP 대신 UDP를 선택했습니다. **실제 Zybo Z7-10 보드 + 노트북 카메라로 End-to-End 통신 검증 완료** (이더넷 다이렉트 연결, 고정 IP).
- **모터 제어 (boat, C++)** — 캐릭터 디바이스 커널 모듈(`motor_driver.c`)이 PS MIO GPIO(MIO10~13)를 직접 제어하고(디바이스 close 시 자동 정지), 유저스페이스 앱(`motor_controller_dev.cpp`)은 `ioctl()`로 방향 명령만 전달합니다. 실물 없이 콘솔 로그만 출력하는 stub 백엔드도 함께 구현해 로직을 먼저 검증할 수 있게 했습니다. 실제 보드에서 커널 모듈 `insmod` 및 GPIO 등록까지 확인했고, 모터 자체의 물리적 배선/구동 테스트는 아직 남아있습니다.
- **초음파 장애물 회피 (boat, C++)** — trigger 핀 토글과 echo 신호의 rising/falling 엣지 측정 모두 커널 모듈(`echo_driver.c`)이 담당합니다. `request_irq`로 엣지 인터럽트를 잡아 펄스 폭을 계산하고, `read()`는 40ms 타임아웃(`wait_event_interruptible_timeout`)이 있어 센서 응답이 없어도 블로킹되지 않고 "장애물 없음"으로 처리됩니다. 유저 앱(`distance_sensor_dev.cpp`)이 이 값을 cm로 환산합니다. 드라이버·인터럽트 로직은 실제 보드에서 검증됐지만, HC-SR04 센서 자체는 아직 물리적으로 배선하지 않았습니다 (echo 라인이 5V 출력이라 3.3V MIO 입력과 직결하면 안 되고 레벨시프터/전압분배 회로가 필요).
- **주행 판단 로직 (boat, C++)** — 초음파 거리가 60cm 미만이면 정지하고, 그렇지 않으면 탐지된 대상 중 confidence가 가장 높은 하나를 골라 프레임 중심과의 x좌표 차이를 비교해 전진/좌회전/우회전을 결정하고, 탐지 결과가 없으면 랜덤 방향 탐색 동작으로 전환합니다.
- **PetaLinux 2017.4 부팅 환경 (Zybo Z7-10)** — Docker 컨테이너에서 PetaLinux 빌드, 커널 모듈 레시피 등록, SD카드 수동 파티셔닝까지 전부 완료되어 실제 보드 부팅 성공. 상세 절차/트러블슈팅은 `document/report/` 참고.
- **Python 레거시 버전 (`BoatControl_py/`)** — 라즈베리파이 한 대에서 동작하던 이전 구조로, 웹캠 영상 처리 + YOLOv5 추론(`yolo_main.py`), 듀얼 모터 제어(`motor_control.py`), HC-SR04 거리 측정(`distance_sensor.py`), 물리 버튼으로 시작/정지(`button_handler.py`) 기능을 포함합니다.

### 아직 남은 작업 (하드웨어 배선 · 모델 위주)

- **초음파 센서(HC-SR04) 물리 배선** — 레벨시프터/전압분배 회로 준비 후 실배선 (echo 5V → MIO 3.3V 변환 필요, 미착수)
- **모터 물리 배선 및 실주행 테스트** — GPIO 신호 자체는 검증됐으나 실제 모터를 붙여서 구동해보는 단계는 아직
- **커스텀 쓰레기 탐지 모델** — 현재는 사전학습 COCO 모델/stub만 사용 중, 실제 학습 데이터로 재학습 필요
- **`boat` 앱 CMake 크로스빌드 재현화** (선택) — 현재는 컨테이너 안 g++ 직접 호출로만 빌드가 검증된 상태

## 아키텍처

캡처·추론을 맡는 노트북과, 수신·제어를 맡는 Zybo Z7-10 보드를 이더넷으로 직결해 UDP로 탐지 결과만 주고받는 구조입니다.

```
[노트북 · Windows·Linux/C++]                              [Zybo Z7-10 · PetaLinux 2017.4/C++]
   vision/                                              boat/
   웹캠 캡처                                              초음파 센서 확인
      │                                          (trigger+echo 전부 커널 모듈(echo_driver)이 직접 소유,
      │                                           echo는 GPIO 인터럽트로 펄스폭 측정, 40ms 타임아웃)
      ▼                                                    (60cm 미만 → 정지)
   ONNX Runtime 추론 (YOLOv10n, 또는 stub)                         │
      │                                                       ▼
      ▼                                                  UDP 수신 (DetectionPacket, port 5555)
   DetectionPacket 구성 ──── UDP(이더넷 직결) ──────────────────▶│
                                                                 ▼
                                                     최고 confidence 대상 선택
                                                                 │
                                                                 ▼
                                                     중심좌표 비교 → 전진/좌/우 판단
                                                                 │
                                                                 ▼
                                                     모터 제어 (motor_driver 커널 모듈, ioctl)
```

| 구성 | 기술 | 역할 |
|---|---|---|
| 영상 캡처/추론 (vision) | OpenCV, ONNX Runtime | 웹캠 캡처, YOLOv10n 추론(또는 stub), UDP 송신 |
| 장치 간 통신 | UDP 소켓 (vision: WinSock / POSIX, boat: POSIX), 포트 5555 | `DetectionPacket` 구조체 그대로 송수신 |
| 하드웨어 제어 (boat) | 커널 캐릭터 디바이스 드라이버(ioctl + interrupt) | 모터 방향 제어(ioctl), 초음파 trigger+echo 인터럽트 전부 커널 모듈이 전담 |
| 빌드 시스템 | CMake(vision, boat/app 각각 독립) + 커널 모듈 Makefile(KDIR 기반) | `USE_ORT`(vision), `USE_HW_MOTOR`/`USE_HW_DISTANCE`(boat) 옵션으로 stub ↔ 실장치 전환 |

## 프로젝트 구조

```
BoatControl/
├─ BoatControl_cpp/                 # C++ 버전 (현재 진행 중)
│  ├─ common/
│  │  └─ detection_packet.h         # vision ↔ boat 공유 UDP 패킷 포맷
│  ├─ vision/                       # 노트북: 웹캠 캡처 + 추론 + UDP 송신
│  │  ├─ inc/                       # detector.h, udp_sender.h
│  │  └─ src/                       # main.cpp, detector_onnxruntime.cpp, detector_stub.cpp, udp_sender.cpp
│  ├─ boat/                         # Zybo Z7-10: UDP 수신 + 모터 제어 + 초음파 센서
│  │  ├─ app/                       # 유저스페이스 앱 (독립 CMake 빌드, PetaLinux 크로스 툴체인)
│  │  │  ├─ inc/                    # motor_controller.h, distance_sensor.h, udp_receiver.h
│  │  │  ├─ src/                    # main.cpp, motor_controller_dev.cpp, motor_controller_stub.cpp,
│  │  │  │                          #   distance_sensor_dev.cpp, distance_sensor.cpp(dummy), udp_receiver.cpp
│  │  │  └─ CMakeLists.txt          # USE_HW_MOTOR / USE_HW_DISTANCE 옵션으로 stub ↔ 실장치 전환
│  │  └─ drivers/                   # 커널 모듈 (KDIR 기반 out-of-tree 빌드)
│  │     ├─ Makefile                # 공통 Makefile (obj-m으로 두 모듈 모두 지정, KDIR/CROSS_COMPILE은 호출 시 지정)
│  │     ├─ motor_driver/           # motor_driver.c, motor_ioctl.h
│  │     └─ echo_driver/            # echo_driver.c
│  └─ CMakeLists.txt                # vision/boat 각각 add_subdirectory (BUILD_VISION / BUILD_BOAT 옵션)
├─ BoatControl_py/                  # Python 버전 (레거시, 라즈베리파이 단일 보드용)
│  ├─ yolo_main.py
│  ├─ motor_control.py
│  ├─ distance_sensor.py
│  └─ button_handler.py
├─ model/                           # (저장소 미포함) yolov10n.onnx를 직접 넣어야 함 — *.onnx는 .gitignore 대상
├─ document/
│  ├─ report/                       # 진행 기록 (report_YYYYMMDD.md) + 빌드/실행 가이드(boat-build-and-run-guide.md)
│  └─ study/                        # 학습 정리 노트 (Python/Conda, PyTorch, OpenMMLab, RT-DETR, 학습,
│                                   #   ONNX/TensorRT, C++ 추론 환경, YOLO 정리·문제해결)
├─ requirements.txt                 # Python 버전용 의존성
├─ .gitattributes / .gitignore
└─ LICENSE
```

> PetaLinux 프로젝트(커널 모듈 레시피, rootfs 설정, u-boot bbappend 등) 자체는 이 저장소와 별도의 Docker 작업 공간(`~/petalinux-work`)에 있습니다 — 이 저장소는 애플리케이션/드라이버 소스만 관리하고, PetaLinux 빌드 시 해당 소스를 크로스컴파일 환경으로 복사해 사용합니다. 정확한 경로/절차는 `document/report/boat-build-and-run-guide.md` 참고.

## 하드웨어 구성

- 노트북 (Windows 또는 Linux, 웹캠) — 영상 캡처 및 추론
- **Zybo Z7-10** (Zynq-7010, PetaLinux 2017.4, Digilent BSP) — 모터·센서 제어
- SD카드 (부팅용, BOOT/rootfs 파티션 수동 구성)
- USB-이더넷 어댑터 (노트북에 내장 유선랜이 없는 경우) + 이더넷 케이블 (두 장비 다이렉트 연결, 고정 IP)
- 듀얼 채널 모터 드라이버 (예: L298N)
- DC 모터 2개
- 초음파 거리 센서 (HC-SR04, 아직 미장착 — 레벨시프터 회로 필요)

## 현재 상태

- **vision (C++)**: 웹캠 캡처 → YOLOv10n(ONNX Runtime) 추론 → UDP 송신까지 빌드 및 동작 확인 완료. `--host`/`--port`/`--camera` CLI 인자 모두 지원. Linux에서 stub 디텍터로 네이티브 빌드해 실제 카메라로 Zybo 보드까지 UDP 전송하는 것을 확인했습니다 (`USE_ORT=ON`으로 실제 ONNX Runtime 추론을 붙이는 것도 CMakeLists상 Windows/Linux 모두 지원하도록 되어 있으나, 커스텀 모델 학습은 아직 진행 전).
- **boat (C++)**: 커널 캐릭터 디바이스 드라이버(`motor_driver`, `echo_driver`) + ioctl/interrupt 기반 구조로 완전히 재설계 완료. **실제 Zybo Z7-10 보드에 PetaLinux 2017.4를 부팅해 커널 모듈 `insmod`, `boat_bin` 실행, `vision`으로부터 UDP 패킷 수신까지 전부 실물 검증했습니다** (2026-09-26). 남은 건 센서/모터 물리 배선입니다.
- **Python 버전 (`BoatControl_py/`)**: 과거 라즈베리파이 단일 보드 구조로 동작했던 레거시 버전으로 저장소에 기록만 남아있고, 현재는 사용하지 않습니다.

## 앞으로 진행할 작업

1. HC-SR04 echo 라인용 레벨시프터/전압분배 회로 준비 후 센서 실배선
2. 모터 드라이버(L298N 등) + DC 모터 실배선, `USE_HW_MOTOR=ON`/`USE_HW_DISTANCE=ON`으로 실제 주행 테스트
3. 커스텀 학습된 쓰레기 탐지 모델로 교체하고, `detector_onnxruntime.cpp`의 전처리/후처리 재점검
4. `vision`을 stub이 아닌 `-DUSE_ORT=ON`으로 빌드해 실제 탐지 기반 방향 제어까지 End-to-End 검증
5. `boat` 앱 CMake 기반 크로스빌드 재현화 (현재는 g++ 직접 호출 방식만 검증됨, 선택 사항)
6. `main.cpp`에 검증용으로 추가한 UDP 수신 디버그 로그 정리(유지 또는 제거) 결정
7. 보드-노트북 네트워크를 고정 IP 다이렉트 연결에서 공유기 경유 등 더 편한 구성으로 바꿀지 검토

---
*이 프로젝트는 친구의 졸업 작품을 돕기 위해 시작되었으며, 임베디드 보드 활용 및 객체 인식 프로그래밍 학습을 주된 목표로 하고 있습니다. 2026-09-26 기준 실제 하드웨어에서 소프트웨어 스택 전체가 검증된 상태이며, 남은 물리적 배선·모델 작업을 이어서 진행할 예정입니다.*
