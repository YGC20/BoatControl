# BoatControl

객체 인식 기술을 활용해 수면의 쓰레기를 자율적으로 수거하는 보트를 만들기 위한 프로젝트입니다.
영상 처리(웹캠 캡처 + ONNX Runtime 추론)와 하드웨어 제어(모터 · 초음파 센서)를 두 장비로 나누어, UDP 소켓으로 탐지 결과를 주고받는 구조로 개발 중입니다.

> ⚠️ **미완성 프로젝트입니다.** 학습 목적으로 진행하던 중 남은 학습·구현량 대비 시간이 부족해 현재 상태로 일단락했습니다. 이 README는 지금까지 구현된 내용과 앞으로 남은 작업을 정리한 것입니다.

## 진행 배경

친구의 졸업 작품을 돕기 위해 시작된 프로젝트로, 라즈베리파이 활용과 객체 인식 프로그래밍 학습을 주된 목표로 삼았습니다.

- 처음에는 Python으로, 라즈베리파이 한 대에서 웹캠 영상 처리(YOLO 추론)와 모터·초음파 센서 제어를 모두 담당하는 구조로 시작했습니다 (`BoatControl_py/`).
- 이후 단순 폴링 방식으로 객체를 탐지하던 기존 구조에서 나아가, C/C++로 객체 탐지 파이프라인과 하드웨어 제어를 직접 구현해보며 학습하기 위해 C++ 버전(`BoatControl_cpp/`)으로 전환했습니다. (아래 [Python → C/C++ 전환 이유](#python--cc-전환-이유) 참고)
- 전환 과정에서 실제로 쓸 수 있는 보드가 라즈베리파이가 아니라 징크보드(Zynq, PetaLinux)로 정해지면서, 노트북(영상 처리)과 징크보드(모터 제어)로 역할을 나누는 2-장비 구조로 다시 설계했습니다.

## Python → C/C++ 전환 이유

기존 Python 버전(`BoatControl_py/`)은 라즈베리파이에서 단순 폴링 방식으로 객체를 탐지하고, 그 결과에 따라 모터를 제어하는 구조였습니다. 이 프로젝트를 통해 C/C++로 객체 탐지와 하드웨어 제어(소켓 통신, GPIO 등)를 직접 구현해보며 배우는 것을 목표로 C++ 버전(`BoatControl_cpp/`)으로 전환을 진행했습니다. Python 버전은 이전 구현 기록으로 저장소에 남겨두었고, 실제 개발은 C++ 버전을 중심으로 진행하고 있습니다.

## 주요 기능

### 구현 완료

- **실시간 객체 탐지 (vision, C++)** — "웹캠 프레임을 캡처해 ONNX Runtime으로 YOLOv8n 모델 추론을 수행하고, 탐지 결과를 매 프레임 UDP로 전송"합니다. 커스텀 학습 모델은 아직 없어 사전학습 COCO 모델로 파이프라인만 검증한 상태입니다.
- **vision ↔ boat UDP 통신 프로토콜** — "탐지 결과(바운딩 박스, 클래스, confidence)를 고정 크기 구조체(`DetectionPacket`)로 패킹해 한 데이터그램 = 한 프레임으로 송수신"합니다. 재전송·순서 보장이 필요 없는 "덮어쓰기" 성격의 제어 데이터라 TCP 대신 UDP를 선택했습니다.
- **모터 제어 (boat, C++)** — "libgpiod로 PS MIO GPIO를 제어해 전진/후진/좌회전/우회전/정지를 수행"하며, 실물 없이 콘솔 로그만 출력하는 stub 백엔드도 함께 구현해 로직을 먼저 검증할 수 있게 했습니다.
- **주행 판단 로직 (boat, C++)** — "탐지된 대상 중 confidence가 가장 높은 하나를 골라 프레임 중심과의 x좌표 차이를 비교해 전진/좌회전/우회전을 결정"하고, 탐지 결과가 없으면 탐색 동작으로 전환합니다.
- **Python 레거시 버전 (`BoatControl_py/`)** — 라즈베리파이 한 대에서 동작하던 이전 구조로, 웹캠 영상 처리 + YOLOv5 추론(`yolo_main.py`), 듀얼 모터 제어(`motor_control.py`), HC-SR04 거리 측정(`distance_sensor.py`), 물리 버튼으로 시작/정지(`button_handler.py`) 기능을 포함합니다.

### 아직 더미/미구현

- **초음파 장애물 회피** — 인터페이스와 판단 로직(20cm 미만이면 정지)은 작성했지만, 실제 센서가 없어 항상 고정값(100cm)을 반환하는 더미 구현만 있습니다.
- **모터 GPIO 핀 배정** — Vivado에서 PS MIO 핀 번호가 아직 확정되지 않아 코드상 placeholder(0~3) 상태입니다.

## 아키텍처

캡처·추론을 맡는 노트북과, 수신·제어를 맡는 징크보드를 LAN 케이블로 직결해 UDP로 탐지 결과만 주고받는 구조입니다.

```
[노트북 · Windows/C++]                              [징크보드 · PetaLinux/C++]
   vision/                                              boat/
   웹캠 캡처                                              초음파 센서 확인
      │                                                    (더미, 20cm 미만 → 정지)
      ▼                                                       │
   ONNX Runtime 추론 (YOLOv8n)                                  ▼
      │                                                  UDP 수신 (DetectionPacket)
      ▼                                                        │
   DetectionPacket 구성 ──── UDP(LAN 직결) ────────────────────▶│
                                                                 ▼
                                                     최고 confidence 대상 선택
                                                                 │
                                                                 ▼
                                                     중심좌표 비교 → 전진/좌/우 판단
                                                                 │
                                                                 ▼
                                                     모터 제어 (libgpiod, PS MIO GPIO)
```

| 구성 | 기술 | 역할 |
|---|---|---|
| 영상 캡처/추론 (vision) | OpenCV, ONNX Runtime | 웹캠 캡처, YOLOv8n 추론, UDP 송신 |
| 장치 간 통신 | UDP 소켓 (WinSock / POSIX) | `DetectionPacket` 구조체 그대로 송수신 |
| 하드웨어 제어 (boat) | libgpiod (PS MIO GPIO) | 모터 방향 제어, 초음파 센서(예정) |
| 빌드 시스템 | CMake | `USE_ORT`(vision), `USE_GPIOD`(boat) 옵션으로 stub ↔ 실장치 전환 |

## 프로젝트 구조

```
BoatControl/
├─ BoatControl_cpp/                 # C++ 버전 (현재 진행 중)
│  ├─ common/
│  │  └─ detection_packet.h         # vision ↔ boat 공유 UDP 패킷 포맷
│  ├─ vision/                       # 노트북: 웹캠 캡처 + 추론 + UDP 송신
│  │  ├─ inc/                       # detector.h, udp_sender.h
│  │  └─ src/                       # main.cpp, detector_onnxruntime.cpp, detector_stub.cpp, udp_sender.cpp
│  ├─ boat/                         # 징크보드: UDP 수신 + 모터 제어 + 초음파(더미)
│  │  ├─ inc/                       # motor_controller.h, motor_pin.h, distance_sensor.h, udp_receiver.h
│  │  └─ src/                       # main.cpp, motor_controller_gpio.cpp, motor_controller_stub.cpp, distance_sensor.cpp, udp_receiver.cpp
│  └─ CMakeLists.txt                # vision만 add_subdirectory (boat는 PetaLinux 크로스 툴체인으로 별도 빌드)
├─ BoatControl_py/                  # Python 버전 (레거시, 라즈베리파이 단일 보드용)
│  ├─ yolo_main.py
│  ├─ motor_control.py
│  ├─ distance_sensor.py
│  └─ button_handler.py
├─ model/                           # YOLOv8n 사전학습 ONNX 모델 (파이프라인 검증용)
├─ document/
│  ├─ report/                       # 진행 기록
│  └─ study/                        # 학습 정리 노트
└─ requirements.txt                 # Python 버전용 의존성
```

## 하드웨어 구성

- 노트북 (Windows, 웹캠) — 영상 캡처 및 추론
- 징크보드 (Zynq, PetaLinux) — 모터·센서 제어
- 듀얼 채널 모터 드라이버 (예: L298N)
- DC 모터 2개
- 초음파 거리 센서 (예: HC-SR04, 아직 미장착)
- LAN 케이블 (두 장비 직결)

## 현재 상태

- **vision (C++)**: 웹캠 캡처 → YOLOv8n(ONNX Runtime) 추론 → UDP 송신까지 빌드 및 동작 확인 완료. `--host`만 CLI 인자로 처리되고 `--port`/`--camera`는 아직 기본값 고정입니다.
- **boat (C++)**: 모터 제어(stub/GPIO), 초음파 더미, UDP 수신, 제어 루프까지 코드는 모두 작성했지만, PetaLinux 크로스컴파일 단계에서 에러가 발생해 원인 파악 전 중단된 상태입니다.
- **Python 버전 (`BoatControl_py/`)**: 과거 라즈베리파이 단일 보드 구조로 동작했던 레거시 버전으로 저장소에 기록만 남아있고, 현재는 사용하지 않습니다.

## 앞으로 진행할 작업

1. Vivado에서 PS MIO 핀 4개(좌우 모터 IN1~IN4) 배정 → `.xsa` export → PetaLinux 하드웨어 설명 갱신
2. PetaLinux rootfs에 `libgpiod`/`libgpiod-dev` 포함 후 SDK 재생성
3. `boat/` 크로스컴파일 에러 원인 파악 및 해결
4. 초음파 센서 실장치 구현 방식 결정(libgpiod edge-event vs 커널 모듈) 및 구현
5. `boat/` 로직을 stub으로 검증한 뒤 `USE_GPIOD=ON` 실장치 테스트
6. 커스텀 학습된 쓰레기 탐지 모델로 교체하고, `detector_onnxruntime.cpp`의 전처리/후처리 재점검
7. `vision/src/main.cpp`의 `--port`, `--camera` CLI 인자 파싱 마무리

---
*이 프로젝트는 친구의 졸업 작품을 돕기 위해 시작되었으며, 라즈베리파이 활용 및 객체 인식 프로그래밍 학습을 주된 목표로 하였습니다. 학습량 대비 시간이 부족해 미완성 상태로 잠정 중단합니다.*
