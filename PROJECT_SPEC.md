# PROJECT_SPEC.md — uav-gcs

> 제24회 한국로봇항공기경연대회 중급부문 멀티콥터형 드론 실내 조난자 탐색 GCS 소프트웨어 기준 문서  
> **이 문서는 팀원과 코딩 에이전트가 공통으로 참조하는 Single Source of Truth입니다.**

최종 수정: 2026-05-21

## 1. 프로젝트 목적

`uav-gcs`는 노트북에서 실행되는 C++ 지상 관제 프로그램이다. 현재 주된
역할은 onboard telemetry/video를 수신하고, onboard가 계산한 비전/격자
metadata를 영상과 로그로 보여 주는 것이다.

현재 구현은 final dashboard가 아니라 안정적인 관제/튜닝 도구다.

- `astroquad-gcs`: 현재 주력 GCS UI. Telemetry, optional MJPEG video,
  overlays, vision/grid log.
- `uav-gcs-telem`: telemetry-only console receiver / development probe.
- `uav-gcs-video`: raw MJPEG viewer.

GCS는 mission 판단을 하지 않는다. Grid mission의 node commit, snake
direction, marker commit은 모두 onboard가 결정한다.

## 2. 책임 범위

| 구분 | 내용 |
|---|---|
| 담당 | UDP telemetry 수신/파싱/표시, UDP MJPEG video 수신/표시, GCS-side overlay, vision log, local grid-map log display, GCS discovery beacon |
| 미담당 | onboard vision detection, mission state decision, Pixhawk low-level control |

분리 원칙:

- GCS는 ArUco/line/intersection detection을 다시 수행하지 않는다.
- GCS는 onboard metadata만 overlay primitive로 변환한다.
- Debug video는 best-effort 관제 채널이다.
- 정확한 video latency는 표시하지 않는다. Pi/Windows clock sync가 없다.

## 3. 현재 구현 상태

| 영역 | 상태 | 구현 위치 |
|---|---|---|
| Basic telemetry receiver | 구현됨 | `src/telemetry_main.cpp`, `src/network/UdpTelemetryReceiver.*` |
| Network config parsing | 구현됨 | `src/common/NetworkConfig.*` |
| Telemetry v1.8 parser | 구현됨 | `src/protocol/TelemetryMessage.*` |
| Packet sequence stats | 구현됨 | `src/protocol/TelemetryMessage.*` |
| Video-only viewer | 구현됨 | `src/video_main.cpp`, `src/app/VideoViewerApp.*` |
| Main GCS receiver | 구현됨 | `src/main.cpp`, `src/app/AstroquadGcsApp.*` |
| UDP MJPEG chunk receiver/reassembler | 구현됨 | `src/video/UdpMjpegReceiver.*`, `src/video/JpegFrameReassembler.*` |
| GCS discovery beacon | 구현됨 | `src/video/GcsDiscoveryBeacon.*` |
| Marker/line/intersection overlays | 구현됨 | `src/overlay/*` |
| OpenCV video backend | 구현됨 | `src/ui/VideoWindow.cpp` |
| Win32/WIC fallback backend | 구현됨 | `src/ui/VideoWindowWin32.cpp` |
| Vision log window/stdout fallback | 구현됨 | `src/ui/VisionLogWindow.*` |
| Frame/telemetry matching store | 구현됨 | `src/telemetry/TelemetryStore.*` |
| Vision/marker log formatting | 구현됨 | `src/telemetry/*Formatter.*` |
| Local grid-map tracker | 구현됨 | `src/telemetry/GridMapTracker.*` |
| `vision.drone_position` parse/store | 구현됨 | parser/app path |
| Mission command sender | 미구현 | planned |
| Full drone/mission dashboard | 미구현 | planned |
| Persistent log subsystem | 미구현 | planned |

현재 ASCII grid map은 committed `vision.grid_node`를 기준으로 그린다.
`vision.drone_position`은 파싱하고 `GridMapTracker`에 전달하지만, renderer는
현재 heading arrow를 fractional sub-cell 위치가 아니라 최신 committed node에
표시한다.

## 4. 전체 시스템 내 위치

```text
Windows/Linux laptop
  └─ uav-gcs
       ├─ UDP telemetry receiver
       ├─ UDP MJPEG debug video receiver
       ├─ GCS-side overlay/log window
       └─ current astroquad-gcs composition root target

Wi-Fi / LAN
  ├─ telemetry UDP 14550: onboard -> GCS
  ├─ command TCP 14551: GCS -> onboard, planned
  ├─ video UDP 5600: onboard -> GCS, optional debug
  └─ discovery UDP 5601: GCS -> LAN broadcast

Raspberry Pi 4 + IMX519 + Pixhawk1
  └─ uav-onboard
       ├─ vision / mission / MAVLink / safety
       └─ telemetry / debug video sender
```

## 5. Current Astroquad GCS Requirements

| 항목 | 요구사항 |
|---|---|
| Telemetry | UDP 14550 JSON 수신, malformed packet drop/count |
| Video | UDP 5600 `AQV1` MJPEG chunks 수신, complete frame만 표시 |
| Discovery | UDP 5601 `AQGCS1 video_port=5600` beacon broadcast |
| Overlay | Onboard metadata로 marker/line/intersection overlay 생성 |
| Grid map | Committed `vision.grid_node` 기반 local ASCII map 렌더 |
| Logs | Packet/video/system/camera/line/intersection/decision/grid-node 표시 |
| Responsiveness | Video receive thread와 UI draw/decode 분리 |
| Metadata-only | Video가 없어도 telemetry/log는 정상 동작 |
| Latency | clock sync 없는 video latency/age 표시 금지 |

## 6. Grid Mission 관제 기준

`grid_mission_node`는 현재 다음 telemetry를 GCS에 보낸다.

- `debug.note = "grid_mission"`
- vision line/intersection/marker metadata
- `vision.intersection_decision`
- 최신 committed `vision.grid_node`
- `vision.drone_position`

주의:

- `grid_mission_node`가 resending하는 `vision.grid_node`는 candidate가 아니라
  onboard mission이 승인한 최신 committed node다.
- GCS는 node id/coordinate 중복을 무시한다.
- GCS는 marker 발견을 표시할 뿐 marker commit 여부를 최종 판단하지 않는다.
- 현재 richer mission object는 telemetry schema에 준비되어 있지만
  `GcsTelemetryPublisher` path에서 아직 채워지지 않는다. Grid state detail은
  onboard console log가 더 정확하다.

## 7. UI 구조

Current UI:

- `astroquad-gcs`: camera window + vision/grid/mission log window.
- `uav-gcs-telem`: console telemetry receiver.
- `uav-gcs-video`: camera window only.
- OpenCV가 있으면 OpenCV highgui backend.
- Windows에서 OpenCV가 없으면 Win32/WIC backend.

Overlay:

- ArUco marker box/corners/center/direction/label.
- Line contour in magenta.
- Red tracking point at camera-center Y and green lateral error line.
- Compact cyan intersection type and yellow branch rays.

Log:

- `[vision]`, `[camera]`, `[system]`, `[packets]`
- `[line]`, `[line-debug]`
- `[intersection]`, `[intersection-decision]`
- `[grid-node]`, fixed top-pane `[grid-map]`
- `[markers]`

Target full dashboard:

```text
[Mission Control] [Drone/System State]
[Grid/Marker Map] [Camera + Vision Overlay]
[Vision Debug]    [Event/Command Log]
```

Dear ImGui/Qt/native Win32 are still future choices; no full dashboard
framework is committed in current code.

## 8. Protocol

공통 protocol 문서:

- `uav-onboard/docs/PROTOCOL.md`
- `uav-gcs/docs/PROTOCOL.md`

현재 문서 version은 v1.8이고 JSON top-level `protocol_version`은 integer `1`이다.

Parser requirements:

- Unknown fields ignored.
- Legacy summary fields and current nested fields tolerated.
- Malformed JSON dropped, not fatal.
- Packet stats track dropped/duplicate/out-of-order packets.

Video rules:

- `AQV1` UDP chunk header + JPEG payload.
- Maximum payload per datagram is 1200 bytes.
- Only complete frames are displayed.
- Last complete frame is retained through temporary UDP drops.

## 9. Directory / File Roles

```text
uav-gcs/
├─ CMakeLists.txt
├─ PROJECT_SPEC.md
├─ README.md
├─ config/
│  ├─ network.toml
│  └─ ui.toml
├─ docs/PROTOCOL.md
├─ src/
│  ├─ app/                  # AstroquadGcsApp, workers, VideoViewerApp
│  ├─ common/               # NetworkConfig
│  ├─ network/              # UdpTelemetryReceiver
│  ├─ overlay/              # Marker/Line/Intersection overlay primitives
│  ├─ protocol/             # TelemetryMessage parser/stats
│  ├─ telemetry/            # TelemetryStore, formatters, GridMapTracker
│  ├─ ui/                   # OpenCV/Win32 video + log windows
│  ├─ video/                # discovery, packet parse, reassembly, receiver
│  ├─ main.cpp              # astroquad-gcs entrypoint
│  ├─ telemetry_main.cpp    # uav-gcs-telem entrypoint
│  └─ video_main.cpp
├─ tests/
└─ tools/
```

Key files:

| File | Role |
|---|---|
| `src/app/AstroquadGcsApp.*` | main GCS composition and UI/log orchestration |
| `src/app/TelemetryWorker.*` | telemetry receive thread, store/grid/marker tracker updates |
| `src/app/VideoReceiveWorker.*` | MJPEG receive thread and latest-frame handoff |
| `src/protocol/TelemetryMessage.*` | v1.8 telemetry parse and sequence stats |
| `src/telemetry/GridMapTracker.*` | committed local grid map rendering/dedup |
| `src/telemetry/VisionLogFormatter.*` | human-readable vision/grid logs |
| `src/overlay/*` | backend-independent overlay primitives |
| `src/video/*` | GCS discovery, MJPEG receive/reassembly |

## 10. Build / Run / Test

Build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Windows Ninja:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
.\build\astroquad-gcs.exe --config config
```

Tests:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Current focused tests:

- `telemetry_line_parse`
- `video_reassembler`
- `line_overlay`
- `intersection_overlay`
- `grid_map_tracker`

## 11. Nonfunctional Requirements

| 항목 | 요구사항 |
|---|---|
| Stability | Unknown fields, malformed packets, incomplete frames must not crash GCS |
| Responsiveness | Socket receive must not be blocked by drawing/decode |
| Cross-platform | Windows first-class; Linux remains buildable with OpenCV |
| Mission separation | GCS visualization must not become required for onboard mission logic |
| Debug honesty | No misleading video latency without synchronized clocks |
| Compatibility | Parser ignores new fields and preserves legacy summaries |

## 12. Safety / Command Requirements

Command channel is planned, not implemented. Reserved command messages:

| Command | 설명 |
|---|---|
| `start_mission` | mission start |
| `abort_mission` | mission abort/return |
| `emergency_land` | immediate landing |
| `set_marker_count` | expected marker count |
| `request_status` | immediate status request |
| `set_control_backend` | backend selection |

Until the command channel is implemented, start/abort/land are handled by
onboard CLI/config, RC takeover, Mission Planner/Pixhawk procedure, and onboard
failsafe paths.

## 13. 개발 우선순위

| 순서 | 작업 | 이유/검증 |
|---:|---|---|
| 1 | `astroquad-gcs` 안정화 유지 | line/grid mission tuning의 관제 도구 |
| 2 | Grid mission telemetry display 확장 | current console-only mission state를 GCS에 구조화 |
| 3 | Mission/drone state model 추가 | dashboard/command ACK 기반 |
| 4 | Command sender channel 구현 | START/ABORT/EMERGENCY LAND/backend 선택 |
| 5 | Persistent log/replay 확장 | SITL/실비행 재현성 |
| 6 | Full dashboard framework 결정 | 최종 운용 UI |

## 14. 금지 / 주의

- GCS에서 local detection을 다시 돌리지 않는다.
- GCS가 uncommitted intersection candidate를 grid node로 승격하지 않는다.
- Debug video frame drop을 mission failure로 해석하지 않는다.
- `vision.drone_position`만으로 grid node를 만들지 않는다.
- Final dashboard 구현 시에도 onboard mission logic을 GCS 의존으로 만들지 않는다.
