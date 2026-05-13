# PROJECT_SPEC.md — uav-gcs

> 제24회 한국로봇항공기경연대회 중급부문 멀티콥터형 드론 실내 조난자 탐색 GCS 소프트웨어 기준 문서  
> **이 문서는 팀원과 코딩 에이전트가 공통으로 참조하는 Single Source of Truth입니다.**

최종 수정: 2026-05-13

---

## 1. 프로젝트 목적

노트북에서 실행되는 C++ 기반 지상 관제 프로그램(GCS)을 개발한다. GCS는 드론의 자율 미션 상태를 실시간으로 시각화하고, 운용자가 고수준 명령을 보낼 수 있는 인터페이스를 제공한다.

현재 레포는 최종 GCS UI 전체 중 **telemetry receiver, raw debug video receiver, GCS-side ArUco/line/intersection overlay, vision log window, local grid-map log display** 단계까지 구현되어 있다. Mission control panel, command sender, drone state dashboard, persistent log subsystem은 목표 아키텍처로 유지하되 아직 구현 전이다.

최종 운용 실행 파일은 Windows 기본 generator 기준 `.\build\Release\uav_gcs.exe`, Ninja 기준 `.\build\uav_gcs.exe`다. 현재 `uav_gcs`는 basic telemetry receiver에 가깝지만, 최종적으로는 mission dashboard, command sender, telemetry/video/logging, safety status를 조립하는 GCS composition root가 되어야 한다. `uav_gcs_vision_debug`는 계속 비전 관제/튜닝용 debug 실행 파일로 유지한다.

72시간 실기체 MVP에서 GCS의 역할은 command UI가 아니라 관제와 기록이다. `uav_gcs_vision_debug`로 line offset, video, telemetry, Pixhawk/onboard state, safety event를 확인하고, mission start/abort/land는 우선 onboard CLI/config, RC takeover, Pixhawk mode/land 절차로 처리한다.

문서 계층:

- 전체 시스템 공통 기준은 `development-log/SYSTEM_SPEC.md`를 따른다.
- 72시간/1주일 MVP 계획은 `development-log/MVP_PLAN.md`를 따른다.
- 이 문서는 `uav-gcs` repo의 책임, 모듈, 실행 파일, 빌드/테스트 기준만 상세히 다룬다.
- `development-log/RESEARCH.md`와 `development-log/PLAN.md`는 매 스텝마다 바뀌는 작업용 scratchpad다.

---

## 2. 이 레포의 책임 범위

| 구분 | 내용 |
|---|---|
| 담당 | Onboard telemetry 수신/파싱/표시, raw debug video 수신/표시, GCS-side marker/line/intersection overlay, vision debug log, GCS discovery beacon, mission UI/command/log/grid map |
| 미담당 | 자율주행 판단은 `uav-onboard` 담당, 저수준 비행 제어는 ArduPilot/Pixhawk 담당, onboard vision detection은 `uav-onboard` 담당 |

중요한 역할 분리:

- GCS는 ArUco/line detection을 로컬에서 다시 수행하지 않는다.
- GCS는 onboard가 보낸 marker/line metadata만 사용해 raw camera 영상 위에 overlay를 그린다.
- Debug video는 best-effort 관제 채널이다. 미션 판단은 telemetry와 onboard mission state를 기준으로 한다.
- Camera window의 video latency/age 표시는 하지 않는다. Pi/Windows clock sync가 보장되지 않아 오해를 만들기 때문이다.

---

## 3. 전체 시스템에서 GCS 위치

```text
Windows/Linux laptop
  └─ uav-gcs
       ├─ UDP telemetry receiver
       ├─ UDP MJPEG debug video receiver
       ├─ GCS-side overlay/log window
       └─ final uav_gcs composition root

Wi-Fi
  ├─ telemetry UDP 14550: onboard -> GCS
  ├─ command TCP 14551: GCS -> onboard, planned
  ├─ video UDP 5600: onboard -> GCS, optional debug
  └─ discovery UDP 5601: GCS -> LAN broadcast

Raspberry Pi 4 + IMX519-78
  └─ uav-onboard
       ├─ camera / vision / telemetry
       └─ mission / MAVLink / safety

UART MAVLink, planned
  └─ Pixhawk / ArduPilot
```

---

## 4. 현재 구현 상태

| 영역 | 상태 | 구현 위치 |
|---|---|---|
| Basic telemetry receiver | 구현됨 | `src/main.cpp`, `src/network/UdpTelemetryReceiver.*` |
| Network config parsing | 구현됨 | `src/common/NetworkConfig.*` |
| Telemetry v1.7 JSON parser | 구현됨 | `src/protocol/TelemetryMessage.*` |
| Telemetry stats | 구현됨 | `src/protocol/TelemetryMessage.*` |
| Video-only viewer | 구현됨 | `src/video_main.cpp`, `src/app/VideoViewerApp.*` |
| Vision debug receiver | 구현됨 | `src/vision_debug_main.cpp`, `src/app/VisionDebugApp.*` |
| UDP MJPEG chunk receiver/reassembler | 구현됨 | `src/video/UdpMjpegReceiver.*`, `src/video/JpegFrameReassembler.*` |
| GCS discovery beacon | 구현됨 | `src/video/GcsDiscoveryBeacon.*` |
| GCS-side line overlay | 구현됨 | `src/overlay/LineOverlay.*` |
| GCS-side marker overlay | 구현됨 | `src/overlay/MarkerOverlay.*` |
| GCS-side intersection overlay | 구현됨 | `src/overlay/IntersectionOverlay.*` |
| OpenCV video window backend | 구현됨 | `src/ui/VideoWindow.cpp` |
| Win32/WIC fallback video backend | 구현됨 | `src/ui/VideoWindowWin32.cpp` |
| Vision log window/stdout fallback | 구현됨 | `src/ui/VisionLogWindow.*` |
| Frame/telemetry matching store | 구현됨 | `src/telemetry/TelemetryStore.*` |
| Vision/marker log formatting | 구현됨 | `src/telemetry/*Formatter.*` |
| Local grid-map log tracker | 구현됨 | `src/telemetry/GridMapTracker.*` |
| Mission control command sender | 미구현 | planned |
| Grid map/mission state UI | 미구현 | `src/state/.gitkeep`, future UI |
| Persistent log subsystem | 미구현 | `src/logging/.gitkeep` |
| Dear ImGui full dashboard | 미구현 | future optional UI direction |

---

## 5. 주요 기능 요구사항

### 5.1 현재 Vision Debug 요구사항

| 항목 | 요구사항 |
|---|---|
| Telemetry | UDP 14550에서 JSON telemetry를 수신하고 malformed packet에도 종료하지 않는다 |
| Video | UDP 5600에서 `AQV1` MJPEG chunks를 받아 complete JPEG frame만 표시한다 |
| Discovery | UDP 5601에 `AQGCS1 video_port=5600` beacon을 broadcast한다 |
| Overlay | Onboard metadata로만 marker/line overlay를 그린다 |
| Log | Vision log window에 packet/video/line/marker/system/camera/debug 정보를 표시한다 |
| Responsiveness | Video receive thread가 socket을 계속 drain하고 UI draw와 분리된다 |
| Debug video off | Camera window가 waiting이어도 telemetry/log가 들어오면 metadata-only 정상 실행으로 본다 |
| Latency display | Video latency/age를 표시하지 않는다 |

### 5.2 최종 Mission GCS 요구사항

| 항목 | 내용 |
|---|---|
| Mission state | IDLE/TAKEOFF/GRID_EXPLORE 등 onboard mission state 표시 |
| Grid map | 현재 격자 좌표, 방문 여부, marker 위치 표시 |
| Command | START/ABORT/EMERGENCY LAND/marker count command 송신 |
| Control backend selection | GUIDED velocity primary, RC override fallback 선택/상태 표시 |
| Drone state | battery, altitude, armed, flight mode, failsafe 표시 |
| Safety events | line lost, GCS lost, low voltage, Pixhawk heartbeat lost 경보 |
| Logging | telemetry/event/command log 저장 및 replay |

### 5.3 Near-Term MVP GCS Scope

3일 MVP에서 GCS는 다음만 필수로 한다.

- `uav_gcs_vision_debug`로 raw camera, overlay, line offset, telemetry log 확인.
- Pixhawk/onboard state가 telemetry로 들어오면 mode, armed, altitude/range, heartbeat health를 표시.
- line lost, heartbeat lost, landing reason 같은 safety event를 로그로 확인.

3일 MVP에서 미루는 것:

- full command sender UI
- marker count command
- backend switching UI
- command ACK/retry UI
- full dashboard framework 결정

---

## 6. UI 구조 방향

### 6.1 Current UI

현재 구현은 빠른 vision bring-up을 위한 경량 UI다.

- `uav_gcs`: 현재 console telemetry receiver, 최종 mission dashboard composition root
- `uav_gcs_video`: camera window only
- `uav_gcs_vision_debug`: camera window + vision log window
- Windows에서 OpenCV가 없어도 Win32/WIC backend로 JPEG decode와 drawing 가능
- OpenCV가 있으면 OpenCV highgui backend 사용 가능

현재 camera window overlay:

- `frame N`
- ArUco marker box/corners/center/direction/label
- Line contour in magenta
- Line tracking point in green

표시하지 않는 것:

- Video latency/age
- Onboard에서 이미 그려진 overlay

### 6.2 Target Full Dashboard

최종 dashboard는 다음 panel 구성을 목표로 한다.

```text
[Mission Control] [Drone/System State]
[Grid/Marker Map] [Camera + Vision Overlay]
[Vision Debug]    [Event/Command Log]
```

Dear ImGui + OpenCV/texture 기반 UI는 여전히 장기 후보지만, 현재 코드에는 Dear ImGui가 포함되어 있지 않다. 현 단계에서는 `uav_gcs_vision_debug`를 안정적인 debug composition으로 유지하고, 최종 `uav_gcs`가 mission control, command sender, drone state, event log를 조립하도록 확장한다. Debug app 코드를 main에 복사하지 않고 공통 telemetry/video/protocol/overlay/logging 모듈을 공유한다.

---

## 7. 통신 구조와 Protocol

현재 공통 protocol 문서:

- `uav-onboard/docs/PROTOCOL.md`
- `uav-gcs/docs/PROTOCOL.md`

현재 문서 version은 v1.7이며 JSON top-level `protocol_version`은 호환성을 위해 integer `1`이다.

| Channel | Direction | Transport | Port | Status |
|---|---|---|---:|---|
| Telemetry | onboard -> GCS | UDP JSON | 14550 | implemented |
| Command | GCS -> onboard | TCP JSON | 14551 | planned |
| Video | onboard -> GCS | UDP MJPEG chunks | 5600 | implemented |
| GCS discovery | GCS -> LAN broadcast | UDP text beacon | 5601 | implemented |

GCS parser requirements:

- Unknown fields are ignored.
- Legacy summary fields and current nested fields are both tolerated where implemented.
- Malformed JSON is dropped and counted, not fatal.
- Packet sequence stats track dropped/duplicate/out-of-order packets.

Video rules:

- `AQV1` UDP chunk header + JPEG payload.
- Maximum payload per datagram is 1200 bytes.
- Only complete frames are displayed.
- Incomplete, old, or mismatched chunks are counted for diagnostics.
- Last complete frame is retained through temporary UDP drops.

---

## 8. 주요 Telemetry 표시 항목

GCS vision log는 다음 정보를 표시한다.

| Group | Fields |
|---|---|
| Vision timing | processing, read, decode, aruco, line, JSON build/send, video submit/send |
| Camera | sensor, index, size, configured/measured FPS, autofocus, lens, exposure, shutter, gain, AWB |
| System | board, OS, uptime, load, memory, throttling, Wi-Fi signal/bitrate |
| Packets/video | received, dropped, duplicate, out-of-order, telemetry bytes, JPEG bytes, chunks, target FPS, sent/skipped/dropped/failures |
| Line | detected, tracking point, offset, angle, confidence, contour points |
| Line debug | raw/filtered/held/rejected, masks, contours, candidates, ROI pixels |
| Markers | count, id, center, orientation |

Video latency/age는 표시하지 않는다. 정확한 end-to-end video latency가 필요하면 clock sync 또는 별도 round-trip 측정 protocol을 설계해야 한다.

---

## 9. 현재 디렉토리 구조와 파일 역할

```text
uav-gcs/
├─ .gitignore
├─ CMakeLists.txt
├─ PROJECT_SPEC.md
├─ README.md
├─ config/
│  ├─ network.toml
│  └─ ui.toml
├─ docs/PROTOCOL.md
├─ logs/.gitkeep
├─ scripts/.gitkeep
├─ src/
│  ├─ app/
│  │  ├─ VideoViewerApp.cpp
│  │  ├─ VideoViewerApp.hpp
│  │  ├─ VisionDebugApp.cpp
│  │  └─ VisionDebugApp.hpp
│  ├─ common/
│  │  ├─ NetworkConfig.cpp
│  │  └─ NetworkConfig.hpp
│  ├─ logging/.gitkeep
│  ├─ main.cpp
│  ├─ network/
│  │  ├─ UdpTelemetryReceiver.cpp
│  │  └─ UdpTelemetryReceiver.hpp
│  ├─ overlay/
│  │  ├─ LineOverlay.cpp
│  │  ├─ LineOverlay.hpp
│  │  ├─ MarkerOverlay.cpp
│  │  ├─ MarkerOverlay.hpp
│  │  └─ OverlayPrimitive.hpp
│  ├─ protocol/
│  │  ├─ TelemetryMessage.cpp
│  │  └─ TelemetryMessage.hpp
│  ├─ state/.gitkeep
│  ├─ telemetry/
│  │  ├─ MarkerLogFormatter.cpp
│  │  ├─ MarkerLogFormatter.hpp
│  │  ├─ TelemetryStore.cpp
│  │  ├─ TelemetryStore.hpp
│  │  ├─ VisionLogFormatter.cpp
│  │  └─ VisionLogFormatter.hpp
│  ├─ ui/
│  │  ├─ VideoWindow.cpp
│  │  ├─ VideoWindow.hpp
│  │  ├─ VideoWindowWin32.cpp
│  │  ├─ VisionLogWindow.cpp
│  │  └─ VisionLogWindow.hpp
│  ├─ video/
│  │  ├─ GcsDiscoveryBeacon.cpp
│  │  ├─ GcsDiscoveryBeacon.hpp
│  │  ├─ JpegFrameReassembler.cpp
│  │  ├─ JpegFrameReassembler.hpp
│  │  ├─ UdpMjpegReceiver.cpp
│  │  ├─ UdpMjpegReceiver.hpp
│  │  ├─ VideoPacket.cpp
│  │  └─ VideoPacket.hpp
│  ├─ video_main.cpp
│  └─ vision_debug_main.cpp
├─ test_data/telemetry/.gitkeep
├─ tests/
│  ├─ CMakeLists.txt
│  ├─ test_line_overlay.cpp
│  ├─ test_telemetry_line_parse.cpp
│  └─ test_video_reassembler.cpp
├─ third_party/.gitkeep
└─ tools/
   ├─ log_replayer.cpp
   └─ mock_onboard.cpp
```

Root/config/docs:

| 파일 | 역할 |
|---|---|
| `CMakeLists.txt` | build graph, dependencies, OpenCV/Win32 backend selection, apps/tools/tests |
| `README.md` | 현행 build/run/vision debug guide |
| `config/network.toml` | onboard address/ports/timeouts |
| `config/ui.toml` | window/layout/video window config. `show_latency=false` |
| `docs/PROTOCOL.md` | onboard/GCS common protocol spec |

Apps:

| 파일 | 역할 |
|---|---|
| `src/main.cpp` | 현재 `uav_gcs` basic telemetry receiver; 최종 GCS runtime composition root로 확장 예정 |
| `src/video_main.cpp` | `uav_gcs_video` CLI entry |
| `src/vision_debug_main.cpp` | `uav_gcs_vision_debug` CLI entry |
| `src/app/VideoViewerApp.*` | video-only app orchestration |
| `src/app/VisionDebugApp.*` | telemetry thread, video thread, overlay, log orchestration |

Core modules:

| 파일 | 역할 |
|---|---|
| `src/common/NetworkConfig.*` | config parsing |
| `src/network/UdpTelemetryReceiver.*` | UDP telemetry socket receive |
| `src/protocol/TelemetryMessage.*` | telemetry parse and stats |
| `src/overlay/*` | backend-independent overlay primitive generation |
| `src/telemetry/TelemetryStore.*` | frame_seq/timestamp based metadata matching |
| `src/telemetry/*Formatter.*` | human-readable vision/marker log formatting |
| `src/ui/VideoWindow.*` | video window interface and OpenCV backend |
| `src/ui/VideoWindowWin32.cpp` | Windows WIC/GDI fallback backend |
| `src/ui/VisionLogWindow.*` | separate log window/stdout fallback |
| `src/video/*` | discovery, UDP MJPEG receive, packet parse, JPEG reassembly |

Tests/tools:

| 파일 | 역할 |
|---|---|
| `tests/test_telemetry_line_parse.cpp` | v1.7 line/intersection/grid-node telemetry parse regression |
| `tests/test_video_reassembler.cpp` | video chunk reassembly/drop stats regression |
| `tests/test_line_overlay.cpp` | line overlay primitive regression |
| `tools/mock_onboard.cpp` | basic telemetry mock sender |
| `tools/log_replayer.cpp` | replay placeholder |

Placeholders:

- `src/logging/.gitkeep`: persistent log subsystem 예정
- `src/state/.gitkeep`: mission/grid/drone state model 예정
- `scripts/.gitkeep`: future build/run helper scripts
- `third_party/.gitkeep`: future vendored dependencies

전체 tracked 파일 인덱스:

```text
.gitignore
CMakeLists.txt
PROJECT_SPEC.md
README.md
config/network.toml
config/ui.toml
docs/PROTOCOL.md
logs/.gitkeep
scripts/.gitkeep
src/app/.gitkeep
src/app/VideoViewerApp.cpp
src/app/VideoViewerApp.hpp
src/app/VisionDebugApp.cpp
src/app/VisionDebugApp.hpp
src/common/.gitkeep
src/common/NetworkConfig.cpp
src/common/NetworkConfig.hpp
src/logging/.gitkeep
src/main.cpp
src/network/.gitkeep
src/network/UdpTelemetryReceiver.cpp
src/network/UdpTelemetryReceiver.hpp
src/overlay/LineOverlay.cpp
src/overlay/LineOverlay.hpp
src/overlay/MarkerOverlay.cpp
src/overlay/MarkerOverlay.hpp
src/overlay/OverlayPrimitive.hpp
src/protocol/.gitkeep
src/protocol/TelemetryMessage.cpp
src/protocol/TelemetryMessage.hpp
src/state/.gitkeep
src/telemetry/MarkerLogFormatter.cpp
src/telemetry/MarkerLogFormatter.hpp
src/telemetry/TelemetryStore.cpp
src/telemetry/TelemetryStore.hpp
src/telemetry/VisionLogFormatter.cpp
src/telemetry/VisionLogFormatter.hpp
src/ui/.gitkeep
src/ui/VideoWindow.cpp
src/ui/VideoWindow.hpp
src/ui/VideoWindowWin32.cpp
src/ui/VisionLogWindow.cpp
src/ui/VisionLogWindow.hpp
src/video/GcsDiscoveryBeacon.cpp
src/video/GcsDiscoveryBeacon.hpp
src/video/JpegFrameReassembler.cpp
src/video/JpegFrameReassembler.hpp
src/video/UdpMjpegReceiver.cpp
src/video/UdpMjpegReceiver.hpp
src/video/VideoPacket.cpp
src/video/VideoPacket.hpp
src/video_main.cpp
src/vision_debug_main.cpp
test_data/telemetry/.gitkeep
tests/CMakeLists.txt
tests/test_line_overlay.cpp
tests/test_telemetry_line_parse.cpp
tests/test_video_reassembler.cpp
third_party/.gitkeep
tools/log_replayer.cpp
tools/mock_onboard.cpp
```

---

## 10. Build, Run, Test

Build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Windows Ninja run:

```powershell
.\build\uav_gcs.exe --config config
.\build\uav_gcs_video.exe --config config
.\build\uav_gcs_vision_debug.exe --config config
```

Windows Visual Studio generator 최종 GCS runtime 목표:

```powershell
.\build\Release\uav_gcs.exe --config config
```

Vision debug normal flow:

```powershell
# laptop
.\build\uav_gcs_vision_debug.exe --config config
```

```bash
# Raspberry Pi, metadata-only
./build/vision_debug_node --config config --line-only --line-mode light_on_dark

# Raspberry Pi, camera/overlay visual debug
./build/vision_debug_node --config config --line-only --line-mode light_on_dark --video
```

Tests:

```powershell
cmake -S . -B build-tests -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

---

## 11. Nonfunctional Requirements

| 항목 | 요구사항 |
|---|---|
| Stability | Unknown telemetry fields, malformed packets, incomplete video frames must not crash GCS |
| UI responsiveness | Socket receive must not be blocked by drawing/decode |
| Cross-platform | Windows is first-class; Linux should remain buildable where OpenCV is available |
| Low operator confusion | Metadata-only mode and video-off state must be clear in logs |
| Mission separation | GCS visualization must not become required for onboard mission logic |
| Debug honesty | Do not show misleading video latency without synchronized clocks |
| Compatibility | Protocol parsers ignore unknown fields and preserve backward-compatible summary fields |

---

## 12. Safety and Command Requirements

Command channel is planned, not implemented. Target final command messages:

| Command | 설명 |
|---|---|
| `start_mission` | mission start |
| `abort_mission` | mission abort/return |
| `emergency_land` | immediate landing |
| `set_marker_count` | expected marker count 설정 |
| `request_status` | immediate status request |
| `set_control_backend` | onboard control backend를 GUIDED velocity primary 또는 RC override fallback 중 선택 |

For the 72-hour line-follow MVP, these commands are not mandatory. GCS should
prioritize observation, logging, and clear safety status. Start/abort/land can
be handled by onboard CLI/config, RC takeover, and Pixhawk mode/land procedures
until the command channel is ready.

UI requirements when implemented:

- Emergency land는 항상 접근 가능하되 오작동 방지를 위한 confirmation이 필요하다.
- Command ACK timeout/retry를 표시해야 한다.
- Command send history는 event log에 남겨야 한다.
- GCS connection lost와 onboard safety event는 눈에 띄게 표시해야 한다.

---

## 13. 개발 우선순위

| 순서 | 작업 | 이유/검증 |
|---:|---|---|
| 1 | `uav_gcs_vision_debug` 안정화 유지 | line/ArUco/intersection tuning의 기본 관제 도구 |
| 2 | `uav_gcs`를 최종 GCS composition root로 확장 | 최종 실행 타깃을 `uav_gcs`로 수렴 |
| 3 | Mission/drone state model 추가 | dashboard와 command ACK의 데이터 중심 |
| 4 | Command sender channel 구현 | START/ABORT/EMERGENCY LAND/backend 선택 |
| 5 | Mission control panel 추가 | 운용 UI 시작점 |
| 6 | Grid/marker map panel 추가 | 대회 미션 관제 핵심 |
| 7 | Event/command log panel 추가 | safety/command 추적 |
| 8 | Telemetry raw log 저장과 replay 도구 확장 | 비행/테스트 재현성 확보 |
| 9 | Full dashboard framework 결정 | Dear ImGui/Qt/기존 Win32 확장 중 선택 |
| 10 | Onboard mission/MAVLink와 SITL/bench 통합 테스트 | 실제 mission loop 검증 |

---

## 14. 향후 확장 가능성

| 항목 | 방향 |
|---|---|
| Protocol 최적화 | JSON 부하가 커지면 MessagePack/FlatBuffers 검토 |
| Full dashboard | 최종 `uav_gcs` composition root에서 mission dashboard로 확장 |
| Multi-run replay | telemetry/video log 기반 test replay |
| 다중 드론 | drone_id 기반 state map으로 확장 |
| Web viewer | GCS protocol layer를 WebSocket bridge로 래핑 |
| Config UI | `config/*.toml` 일부 값을 GCS에서 편집/전송 |
