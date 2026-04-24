# PROJECT_SPEC.md — uav-gcs

> 제24회 한국로봇항공기경연대회 중급부문 멀티콥터형 드론 실내 조난자 탐색 GCS 소프트웨어 기준 문서  
> **이 문서는 팀원과 코딩 에이전트가 공통으로 참조하는 Single Source of Truth입니다.**

---

## 1. 프로젝트 목적

노트북에서 실행되는 C++ 기반 지상 관제 프로그램(GCS)을 개발한다.  
GCS는 드론의 자율 미션 상태를 실시간으로 시각화하고, 운용자가 고수준 명령을 보낼 수 있는 인터페이스를 제공한다.  
드론의 자율주행 판단은 온보드 프로그램이 수행하며, GCS는 **모니터링과 비상 대응**에 집중한다.

---

## 2. 이 레포의 책임 범위

| 담당 | 내용 |
|------|------|
| ✅ 담당 | 온보드 telemetry 수신·표시, 미션 상태 시각화, 격자·마커 맵 표시, vision debug 정보 표시, 고수준 명령 송신, 로그 저장·확인, 영상 스트림 표시 |
| ❌ 미담당 | 자율주행 판단 (온보드 담당), 저수준 비행 제어 (ArduPilot 담당), 온보드 비전 처리 (uav-onboard 담당) |

---

## 3. 전체 드론 시스템에서 GCS의 위치

```
┌─────────────────────────────────────────────────────────┐
│                     노트북 (GCS)                         │
│                                                          │
│  ┌──────────────────────────────────────┐               │
│  │           uav-gcs (이 레포)           │               │
│  │  UI / Telemetry수신 / Command송신     │               │
│  └──────────────┬──────────────┬────────┘               │
│                 │ UDP/TCP      │ Video Stream            │
└─────────────────┼──────────────┼─────────────────────────┘
                  │              │  Wi-Fi
┌─────────────────┼──────────────┼─────────────────────────┐
│          Raspberry Pi Zero 2 W │                          │
│                 │              │                          │
│  ┌──────────────▼──────────────▼────────┐               │
│  │         uav-onboard                  │               │
│  │  Vision / Mission / Control /        │               │
│  │  Autopilot / Telemetry / Safety      │               │
│  └──────────────────────┬───────────────┘               │
│                         │ UART MAVLink                   │
└─────────────────────────┼───────────────────────────────┘
                          │
                  ┌───────▼────────┐
                  │ Pixhawk 1      │
                  │ (ArduPilot)    │
                  └────────────────┘
```

---

## 4. 주요 기능 요구사항

### 4.1 모니터링 (수신·표시)

| 항목 | 내용 |
|------|------|
| 미션 상태 | 현재 MissionState (IDLE / TAKEOFF / GRID_EXPLORE 등) |
| 격자 정보 | 현재 격자 좌표 (row, col), heading, 방문 여부 |
| 마커 맵 | 발견된 ArUco 마커 ID와 격자 좌표 목록 |
| Vision Debug | line_offset, line_angle, intersection_score, marker_id 등 |
| 드론 상태 | 고도, 배터리 전압/%, armed 상태, flight mode, failsafe |
| Safety 이벤트 | line lost, GCS 통신 두절, 배터리 저전압 등 알림 |
| 영상 | 카메라 영상 스트림 (별도 프로세스 또는 통합) |

### 4.2 명령 (송신)

| 명령 | 설명 |
|------|------|
| CMD_START | 미션 시작 |
| CMD_ABORT | 미션 중단 후 복귀 |
| CMD_EMERGENCY_LAND | 즉시 비상 착륙 |
| CMD_SET_MARKER_COUNT | 탐색할 마커 총 개수 설정 |

### 4.3 로그

- 수신된 telemetry 로그를 파일로 저장
- Safety 이벤트 및 명령 송신 이력 기록
- GCS 실행 중 확인 가능한 이벤트 로그 뷰

---

## 5. 비기능 요구사항

| 항목 | 요구사항 |
|------|----------|
| 안정성 | 알 수 없는 telemetry 필드가 추가되어도 GCS가 크래시되지 않아야 함 |
| 프로토콜 버전 관리 | 모든 메시지에 `protocol_version` 필드 포함, 버전 불일치 시 경고 표시 |
| 지연 허용 | telemetry 1~2초 지연까지 정상으로 간주, 초과 시 연결 끊김 표시 |
| 운용 환경 | Windows / Linux 노트북 양쪽에서 빌드 가능 |
| UI 반응성 | 렌더링 루프가 네트워크 수신에 블로킹되지 않아야 함 |
| Headless 없음 | GCS는 GUI 실행이 기본 (온보드와 반대) |
| 빠른 개발 | 초기 MVP에서는 단순한 UI와 JSON 프로토콜로 시작 |

---

## 6. UI 화면 구성 제안

### 6.1 권장 UI 프레임워크: **Dear ImGui + OpenCV**

| 후보 | 장점 | 단점 |
|------|------|------|
| **Dear ImGui** | 빠른 개발, 크로스플랫폼, C++ 네이티브, 즉시 렌더링 | 고급 위젯 부족 |
| Qt | 완성도 높은 위젯, 크로스플랫폼 | 빌드 복잡, 라이선스 |
| OpenCV highgui | 영상 표시 간단 | UI 기능 빈약 |
| TUI (ncurses) | 초경량 | 영상 표시 불가 |

**결론**: Dear ImGui를 기본 UI로 사용하고, 영상 표시는 OpenCV 텍스처로 ImGui 창에 통합한다.  
렌더링 백엔드는 SDL2 또는 GLFW + OpenGL을 사용한다.

### 6.2 화면 레이아웃

```
┌─────────────────────────────────────────────────────────────┐
│  [미션 제어 패널]          [드론 상태 패널]                   │
│  - Marker Count 설정       - 고도 / 배터리 / Armed           │
│  - START / ABORT /         - Flight Mode / Failsafe          │
│    EMERGENCY LAND 버튼     - 연결 상태 (온보드 / Pixhawk)    │
├──────────────────────┬──────────────────────────────────────┤
│  [격자 맵 뷰]         │  [카메라 영상]                       │
│  - 방문 교차점 표시   │  - 하향 카메라 스트림                │
│  - 마커 위치/ID 표시  │  - Vision debug overlay 옵션         │
│  - 현재 위치 강조     │                                      │
├──────────────────────┴──────────────────────────────────────┤
│  [Vision Debug 패널]       [이벤트 로그 뷰]                  │
│  - line_offset / angle     - 최신 이벤트 스크롤              │
│  - intersection_score      - Safety 이벤트 강조              │
│  - marker_id 검출 여부     - 명령 송신 이력                  │
└─────────────────────────────────────────────────────────────┘
```

---

## 7. 온보드 프로그램과의 통신 구조

### 7.1 통신 방식 비교 및 권장안

| 방식 | 장점 | 단점 | 평가 |
|------|------|------|------|
| **UDP** | 저지연, 구현 단순 | 신뢰성 없음 (패킷 손실) | ✅ telemetry에 적합 |
| TCP | 신뢰성 보장 | 지연 발생 가능 | ✅ command에 적합 |
| WebSocket | 브라우저 연동 가능 | 의존성 증가 | ❌ C++ 네이티브에 불필요 |
| ZeroMQ | pub/sub, 유연 | 의존성, 복잡도 | △ 나중 확장 시 고려 |

**권장**: **telemetry는 UDP**, **command는 TCP**로 분리한다.
- UDP telemetry: 패킷 손실을 허용하되, sequence number로 손실 감지
- TCP command: 명령 수신 확인(ACK) 보장

### 7.2 포트 구성 (설정 파일로 변경 가능)

| 채널 | 방향 | 방식 | 기본 포트 |
|------|------|------|-----------|
| Telemetry | 온보드 → GCS | UDP | 14550 |
| Command | GCS → 온보드 | TCP | 14551 |
| Video | 온보드 → GCS | UDP (MJPEG 또는 H.264) | 5600 |

---

## 8. Telemetry Message 설계 방향

### 8.1 초기 버전: JSON 기반

```json
{
  "protocol_version": 1,
  "msg_type": "STATUS",
  "seq": 12345,
  "timestamp_us": 1714000000000000,
  "mission_state": "GRID_EXPLORE",
  "grid_pos": { "row": 2, "col": 3 },
  "heading_deg": 90.0,
  "altitude_m": 1.2,
  "battery_voltage": 11.8,
  "battery_pct": 72,
  "armed": true,
  "flight_mode": "GUIDED",
  "failsafe": false
}
```

```json
{
  "protocol_version": 1,
  "msg_type": "MARKER_FOUND",
  "seq": 12346,
  "timestamp_us": 1714000001000000,
  "marker_id": 3,
  "grid_pos": { "row": 2, "col": 3 }
}
```

```json
{
  "protocol_version": 1,
  "msg_type": "VISION_DEBUG",
  "seq": 12347,
  "timestamp_us": 1714000001500000,
  "line_offset": -12.5,
  "line_angle": 1.2,
  "intersection_score": 0.87,
  "marker_id": -1
}
```

```json
{
  "protocol_version": 1,
  "msg_type": "SAFETY_EVENT",
  "seq": 12348,
  "timestamp_us": 1714000002000000,
  "event": "LINE_LOST",
  "detail": "line not detected for 2.0s"
}
```

### 8.2 메시지 타입 목록

| msg_type | 주기/이벤트 | 내용 |
|----------|-------------|------|
| STATUS | 1Hz | 드론 전반 상태 |
| GRID_MAP | 이벤트 | 방문한 교차점 전체 목록 |
| MARKER_FOUND | 이벤트 | 새 마커 발견 |
| MARKER_MAP | 이벤트 | 마커 전체 목록 |
| VISION_DEBUG | 5~10Hz | Vision 측정값 |
| SAFETY_EVENT | 이벤트 | Safety 경보 |
| LOG | 이벤트 | 온보드 로그 메시지 |
| MISSION_STATE_CHANGE | 이벤트 | 상태머신 전환 |

### 8.3 향후 전환 방향

JSON → **FlatBuffers** 또는 **MessagePack** (성능 부족 시)  
프로토콜 버전 필드(`protocol_version`)를 유지하면 하위 호환 가능

---

## 9. Command Message 설계 방향

```json
{
  "protocol_version": 1,
  "cmd_type": "CMD_START",
  "seq": 1,
  "timestamp_us": 1714000000000000
}
```

```json
{
  "protocol_version": 1,
  "cmd_type": "CMD_SET_MARKER_COUNT",
  "seq": 2,
  "timestamp_us": 1714000000000000,
  "params": { "marker_count": 5 }
}
```

### Command ACK (온보드 → GCS)

```json
{
  "protocol_version": 1,
  "msg_type": "CMD_ACK",
  "seq": 2,
  "ack_seq": 2,
  "result": "OK",
  "detail": ""
}
```

| cmd_type | 설명 |
|----------|------|
| CMD_START | 미션 시작 |
| CMD_ABORT | 미션 중단 후 복귀 |
| CMD_EMERGENCY_LAND | 즉시 비상 착륙 |
| CMD_SET_MARKER_COUNT | 탐색 마커 수 설정 |

---

## 10. Video Stream 처리 방향

### 초기 단계 (MVP)
- 온보드에서 `libcamera-vid` 또는 OpenCV `VideoCapture`로 MJPEG UDP 스트리밍
- GCS에서 OpenCV `VideoCapture("udp://...")` 로 수신
- ImGui 창에 OpenCV 프레임을 OpenGL 텍스처로 렌더링

### 향후 단계
- GStreamer 파이프라인으로 H.264 인코딩 스트리밍
- Vision debug overlay (line, intersection, marker bounding box) 옵션 추가

### 분리 원칙
- 영상 스트림은 telemetry/command 채널과 **완전히 분리**된 UDP 포트 사용
- 영상 수신 스레드가 UI 렌더링을 블로킹하지 않도록 별도 스레드로 구성

---

## 11. Logging 요구사항

| 항목 | 내용 |
|------|------|
| Telemetry 로그 | 수신된 모든 JSON 패킷을 타임스탬프와 함께 파일 저장 |
| 이벤트 로그 | 마커 발견, 상태 전환, Safety 이벤트, 명령 송신 이력 |
| GCS 로그 | GCS 내부 오류, 연결 상태 변화 |
| 파일 이름 | `logs/gcs_YYYYMMDD_HHMMSS.log` |
| 형식 | 텍스트 (행당 `[타임스탬프] [레벨] 메시지`) |
| UI 뷰 | GCS 화면 내 이벤트 로그 스크롤 뷰 (최근 N줄) |

---

## 12. Safety/Emergency Command 요구사항

| 요구사항 | 내용 |
|----------|------|
| 비상 버튼 위치 | UI에서 항상 눈에 잘 띄는 위치에 배치, 빨간색 강조 |
| 오작동 방지 | EMERGENCY LAND는 확인 다이얼로그 또는 더블클릭으로 보호 |
| 연결 상태 표시 | 온보드와 연결이 끊기면 UI에 명확히 표시 |
| 명령 재전송 | CMD_EMERGENCY_LAND는 ACK 수신 전까지 N회 재전송 |
| Safety 이벤트 강조 | SAFETY_EVENT 수신 시 UI에 경보 표시 (색상 변화 등) |
| 명령 이력 | 모든 송신 명령과 ACK 결과를 로그에 기록 |

---

## 13. 권장 디렉토리 구조

아래 구조는 **UI 확장성**, **통신 안정성**, **팀 분업**, **유지보수성**을 고려하여 설계되었다.

```
uav-gcs/
│
├── CMakeLists.txt              # 루트 빌드 파일
├── PROJECT_SPEC.md             # 이 문서
├── README.md                   # 빌드·실행 빠른 안내
│
├── config/                     # 런타임 설정 파일
│   ├── network.toml            # 온보드 IP, 포트 설정
│   └── ui.toml                 # 창 크기, 레이아웃 옵션
│
├── src/                        # 소스 코드
│   ├── main.cpp                # 진입점: 초기화, 메인 루프
│   │
│   ├── network/                # 네트워크 통신 레이어
│   │   ├── TelemetryReceiver.hpp   # UDP telemetry 수신
│   │   ├── TelemetryReceiver.cpp
│   │   ├── CommandSender.hpp       # TCP command 송신
│   │   ├── CommandSender.cpp
│   │   ├── VideoReceiver.hpp       # UDP 영상 수신 (OpenCV)
│   │   └── VideoReceiver.cpp
│   │
│   ├── protocol/               # 메시지 직렬화·역직렬화
│   │   ├── Messages.hpp            # 모든 메시지 타입 정의
│   │   ├── TelemetryParser.hpp     # JSON → 구조체 변환
│   │   ├── TelemetryParser.cpp
│   │   ├── CommandSerializer.hpp   # 구조체 → JSON 변환
│   │   └── CommandSerializer.cpp
│   │
│   ├── state/                  # GCS 내부 상태 관리
│   │   ├── DroneState.hpp          # 수신된 드론 상태 보관
│   │   ├── DroneState.cpp
│   │   ├── GridMapState.hpp        # 격자 맵·마커 맵 보관
│   │   └── GridMapState.cpp
│   │
│   ├── ui/                     # Dear ImGui 기반 UI
│   │   ├── AppWindow.hpp           # 최상위 ImGui 앱 루프
│   │   ├── AppWindow.cpp
│   │   ├── panels/
│   │   │   ├── MissionControlPanel.hpp   # START/ABORT/EMERG 버튼
│   │   │   ├── MissionControlPanel.cpp
│   │   │   ├── DroneStatusPanel.hpp      # 고도/배터리/모드
│   │   │   ├── DroneStatusPanel.cpp
│   │   │   ├── GridMapPanel.hpp          # 격자 맵 시각화
│   │   │   ├── GridMapPanel.cpp
│   │   │   ├── VisionDebugPanel.hpp      # Vision 측정값
│   │   │   ├── VisionDebugPanel.cpp
│   │   │   ├── VideoPanel.hpp            # 카메라 영상
│   │   │   ├── VideoPanel.cpp
│   │   │   ├── EventLogPanel.hpp         # 이벤트 로그 뷰
│   │   │   └── EventLogPanel.cpp
│   │   └── widgets/
│   │       ├── ConnectionIndicator.hpp   # 연결 상태 표시 위젯
│   │       └── EmergencyButton.hpp       # 비상 버튼 (보호 포함)
│   │
│   ├── logging/                # GCS 로그 기록
│   │   ├── GcsLogger.hpp
│   │   └── GcsLogger.cpp
│   │
│   └── common/                 # 공통 유틸리티
│       ├── Config.hpp              # 설정 파일 파서
│       ├── Config.cpp
│       └── Types.hpp               # 공통 데이터 구조
│
├── tests/                      # 단위 테스트 (Google Test)
│   ├── CMakeLists.txt
│   ├── protocol/
│   │   ├── test_telemetry_parser.cpp
│   │   └── test_command_serializer.cpp
│   └── state/
│       └── test_grid_map_state.cpp
│
├── tools/                      # 개발·디버그 도구
│   ├── mock_onboard.cpp        # 온보드 없이 telemetry 생성 (테스트용)
│   └── log_replayer.cpp        # 저장된 로그를 재생해 UI 동작 확인
│
├── scripts/                    # 빌드·실행 스크립트
│   ├── build.sh                # 빌드 자동화
│   └── run_gcs.sh              # 설정 파일 경로 전달 후 실행
│
├── docs/                       # 추가 설계 문서
│   ├── protocol_messages.md    # 메시지 타입 전체 명세
│   └── ui_layout.md            # UI 레이아웃 상세 설명
│
└── logs/                       # 런타임 로그 (gitignore)
    └── .gitkeep
```

### 구조 설계 근거

| 결정 | 이유 |
|------|------|
| `network/`와 `protocol/` 분리 | 통신 방식(UDP/TCP) 교체 시 protocol 레이어 영향 없음 |
| `state/` 별도 분리 | UI 패널들이 DroneState를 공유 참조, 네트워크 코드와 독립 |
| `ui/panels/`와 `ui/widgets/` 분리 | 패널은 기능 단위, 위젯은 재사용 가능한 UI 컴포넌트 |
| `tools/mock_onboard` | 드론 없이 노트북만으로 GCS UI 개발·테스트 가능 |
| `tools/log_replayer` | 비행 후 로그를 재생해 UI 동작 검증 가능 |
| `config/` toml 파일 | IP·포트를 코드 재빌드 없이 현장에서 변경 가능 |

---

## 14. 빌드/실행 방향

### 의존성

| 라이브러리 | 용도 | 설치 |
|------------|------|------|
| Dear ImGui | UI 렌더링 | 소스 내 포함 또는 서브모듈 |
| GLFW3 | OpenGL 창 관리 | 패키지 매니저 |
| OpenGL | 렌더링 백엔드 | 시스템 제공 |
| OpenCV 4.x | 영상 수신·표시 | 패키지 매니저 |
| nlohmann/json | JSON 파싱 | 헤더 온리, 포함 |
| Google Test | 단위 테스트 | 패키지 매니저 |

### 빌드

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### 실행

```bash
./uav_gcs --config ../config/
```

### 단위 테스트

```bash
cmake .. -DBUILD_TESTS=ON
make -j$(nproc)
ctest --output-on-failure
```

### Mock 온보드 실행 (드론 없이 UI 테스트)

```bash
./tools/mock_onboard --gcs-ip 127.0.0.1
```

---

## 15. 개발 우선순위

| 단계 | 작업 | 검증 방법 |
|------|------|-----------|
| 1 | `common/Types.hpp` — 공통 데이터 구조 정의 | 코드 리뷰 |
| 2 | `protocol/` — JSON 파싱·직렬화 | 단위 테스트 |
| 3 | `network/TelemetryReceiver` — UDP 수신 | mock_onboard로 수신 확인 |
| 4 | `network/CommandSender` — TCP 송신 | mock_onboard ACK 확인 |
| 5 | `state/DroneState`, `GridMapState` — 상태 보관 | 단위 테스트 |
| 6 | `ui/AppWindow` + Dear ImGui 기본 루프 | 빈 창 실행 확인 |
| 7 | `ui/panels/DroneStatusPanel` | mock 데이터로 표시 확인 |
| 8 | `ui/panels/MissionControlPanel` | 명령 송신 및 ACK 확인 |
| 9 | `ui/panels/GridMapPanel` | 격자·마커 시각화 확인 |
| 10 | `ui/panels/VisionDebugPanel` | Vision 수치 표시 확인 |
| 11 | `network/VideoReceiver` + `ui/panels/VideoPanel` | 영상 스트림 표시 |
| 12 | `logging/GcsLogger` | 로그 파일 생성 확인 |
| 13 | `tools/log_replayer` | 로그 재생 UI 확인 |
| 14 | 온보드와 통합 테스트 | 실제 드론 연결 |

---

## 16. 향후 확장 가능성

| 항목 | 방향 |
|------|------|
| 프로토콜 전환 | JSON → MessagePack 또는 FlatBuffers (성능 부족 시) |
| 다중 드론 | `DroneState`를 `drone_id` 기반 맵으로 확장 |
| 웹 기반 GCS | `protocol/` 레이어를 WebSocket 서버로 래핑 |
| 미션 기록 재생 | `log_replayer` 확장으로 전체 미션 시각적 재생 |
| 알림 소리 | Safety 이벤트 발생 시 경보음 출력 |
| 설정 UI | config 파일을 GCS 내에서 편집 가능하도록 |
| ZeroMQ | pub/sub 구조로 전환 시 다중 구독자 지원 가능 |

---

*최종 수정: 2026-04-24 | 작성: 윤민석*
