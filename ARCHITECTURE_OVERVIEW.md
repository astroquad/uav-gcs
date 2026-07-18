# astroquad-gcs 아키텍처 개요

> 현재 소스/protocol v1.13 기준 검토: 2026-07-18. 실비행 운영과 사후 로그
> 분석은 [`../development-log/REAL_FLIGHT_ONBOARDING.md`](../development-log/REAL_FLIGHT_ONBOARDING.md)를 따른다.

이 문서는 `astroquad-gcs` 실행 파일이 어떻게 구성되어 있고, 온보드(드론)에서
보낸 텔레메트리/영상 패킷이 어떤 경로로 흘러서 화면에 그려지는지를 처음 읽는
사람이 한눈에 파악할 수 있도록 정리한 코드 리딩 가이드다.

세부 파서 스펙이나 UI 구현은 다루지 않는다. 그건 [PROJECT_SPEC.md](PROJECT_SPEC.md)
와 [docs/PROTOCOL.md](docs/PROTOCOL.md), 각 모듈의 헤더/구현 파일을 참고하면 된다.

---

## 1. 전체 GCS 아키텍처

`astroquad-gcs`는 두 개의 백그라운드 수신 스레드 + 한 개의 메인(UI) 루프로
구성된 멀티스레드 구조다. 수신 스레드가 네트워크 입출력을 처리하고, 메인
루프는 최신 상태만 꺼내 화면을 갱신한다.

```text
                              ┌────────────────────────────────────────────┐
                              │              astroquad-gcs                   │
                              │     (노트북에서 도는 GCS 프로세스)          │
                              └────────────────────────────────────────────┘

  [드론 onboard] ──UDP 14550 (JSON telemetry)──▶ ┌──────────────────────┐
                                                 │  TelemetryWorker      │  (수신 스레드)
                                                 │  ─ UdpTelemetryReceiver│
                                                 │  ─ parseTelemetryJson │
                                                 └──────────┬───────────┘
                                                            │ TelemetryMessage
                                                            ▼
                                       ┌────────────────────────────────────┐
                                       │ TelemetryStore  / GridMapTracker  /│
                                       │ MarkerTracker (스레드 안전 저장소) │
                                       └────────────────────────────────────┘
                                                            ▲
                                                            │ 메인 루프가 읽음
  [드론 onboard] ──UDP 5600 (MJPEG chunks)──▶  ┌──────────────────────┐
                                                 │ VideoReceiveWorker    │  (수신 스레드)
                                                 │  ─ UdpMjpegReceiver   │
                                                 │  ─ JpegFrameReassembler│
                                                 └──────────┬───────────┘
                                                            │ JpegFrame (재조립 완료)
                                                            ▼
                                                ┌──────────────────────┐
                                                │  latest_frame_ 슬롯   │
                                                └──────────┬───────────┘
                                                            │
                                                            ▼
              ┌────────────────────────────────────────────────────────────────┐
              │                AstroquadGcsApp::run() 메인 루프                 │
              │                                                                 │
              │  while (true):                                                  │
              │    frame = video_worker.takeLatestFrame()                       │
              │    matched = telemetry_store.findForFrame(frame_id, ts)         │
              │    overlays = buildLine/Intersection/MarkerOverlays(matched)   │
              │    VideoWindow.showFrame(frame, overlays)         ─── 영상 출력 │
              │    VisionLogWindow.update(grid_text, markers_text, detail)   ─ 로그│
              └────────────────────────────────────────────────────────────────┘
                                                            │
                                                            ▼
                       ┌─────────────────────┐    ┌─────────────────────┐
                       │   VideoWindow        │    │  VisionLogWindow    │
                       │ (OpenCV highgui or   │    │ (격자 맵 / 마커     │
                       │  Win32+WIC fallback) │    │  / vision 로그 패널)│
                       └─────────────────────┘    └─────────────────────┘


  [GcsDiscoveryBeacon] ──UDP 5601 broadcast──▶ [LAN의 onboard]
        (GCS가 살아있다고 알려서 onboard가 video 송신 대상 IP를 잡게 함)
```

핵심 원칙:

- **GCS는 미션 판단을 하지 않는다.** ArUco/라인 검출, 노드 commit, 마커 commit은
  전부 onboard가 한다. GCS는 onboard가 보낸 metadata를 시각화만 한다.
- **수신과 렌더링은 분리되어 있다.** 텔레메트리/영상 수신은 각각 별도 스레드.
  메인 루프가 디코딩으로 막혀도 소켓은 계속 읽힌다.
- **영상이 없어도 텔레메트리/로그는 정상 동작**한다. Debug video는 부가 채널.
- **불완전한 프레임은 표시하지 않는다.** MJPEG가 청크 단위로 도착하고, 완성된
  것만 화면에 띄운다.

---

## 2. 메인 프로그램 실행 흐름

엔트리 포인트부터 메인 루프까지, 호출되는 주요 함수와 객체를 흐름순으로 정리한다.

### --- src/main.cpp ---

```cpp
int main(int argc, char** argv) {
    Options options = parseOptions(argc, argv);             // CLI 옵션 파싱
    auto network_config = gcs::common::loadNetworkConfig(...); // config/network.toml
    loadUiConfig(...);                                       // config/ui.toml

    gcs::app::AstroquadGcsApp app;       // 앱 객체 선언
    return app.run(app_options);         // 모든 로직은 run() 안에서
}
```

`main`은 옵션 파싱과 config 로딩만 한다. 실제 GCS 동작은 전부
`AstroquadGcsApp::run`에 있다.

### --- src/app/AstroquadGcsApp.cpp ---

#### `run(options)` — GCS 전체의 조립자(composition root)

크게 보면 **준비 → 메인 루프 → 종료** 3단계다.

##### (1) 준비 단계

```text
VideoReceiveWorker video_worker
video_worker.start(video_port, timeout_ms)        // 영상 수신 스레드 시작 (UDP 5600)

TelemetryStore telemetry_store                    // 프레임-텔레메트리 매칭 저장소
GridMapTracker grid_map_tracker                   // committed grid_node 누적/렌더
MarkerTracker  marker_tracker                     // 발견 마커 정보 누적

TelemetryWorker telemetry_worker
telemetry_worker.start(telemetry_port, ..., store, grid, marker)
                                                  // 텔레메트리 수신 스레드 시작 (UDP 14550)

GcsDiscoveryBeacon beacon
beacon.start(video_port)                          // UDP 5601로 GCS 존재 광고

VideoWindow window(title)                         // 영상 출력 창 (OpenCV 또는 Win32)
VisionLogWindow log_window("Astroquad GCS Log")   // 로그/격자/마커 패널 창
```

##### (2) 메인 루프 (한 프레임 또는 주기마다 반복)

```text
while (true) {

    // ── 영상 처리 ────────────────────────────────────────────────
    frame = video_worker.takeLatestFrame()                 // 최신 완성 프레임 꺼내기
    if (frame) {
        marker_frame = telemetry_store.findForFrame(       // 같은 frame_id/ts의 텔레메트리 찾기
                          frame.frame_id, frame.timestamp_ms)
        overlays = []
        overlays += buildLineOverlays(marker_frame.line)
        overlays += buildIntersectionOverlays(...)
        overlays += buildMarkerOverlays(marker_frame.markers)

        window.showFrame(frame, overlays)                   // 영상 + overlay 렌더링
    } else {
        // 일정 시간 안 들어오면 "waiting for video stream..." 표시
    }

    // ── 에러/통계 처리 ─────────────────────────────────────────────
    video_worker.takeLastError()                            // 영상 수신 경고
    telemetry_worker.takeLastError()                        // 텔레메트리 수신 경고
    display_fps 계산                                         // 1초 단위 FPS 갱신

    // ── 로그 갱신 (기본 1.5초마다) ─────────────────────────────────
    if (marker_log_interval_ms 지났으면) {
        grid_text    = grid_map_tracker.render()            // 격자 맵 ASCII
        markers_text = marker_tracker.render()              // 마커 리스트
        detail       = formatVisionLog(latest, mission, stats) +
                       formatVideoStatsLine(...)
        log_window.update(grid_text, markers_text, detail)  // 로그 창 갱신
    }

    log_window.poll()                                       // 로그 창 이벤트 처리
    if (window.shouldClose(1)) break                        // q/ESC 누르면 종료
}
```

##### (3) 종료 단계

```text
video_worker.stop()                                 // 영상 수신 스레드 종료
telemetry_worker.stop()                             // 텔레메트리 수신 스레드 종료
```

---

### 백그라운드 스레드의 동작

#### --- src/app/TelemetryWorker.cpp ---

```text
worker thread:
  while (running):
      UdpTelemetryReceiver.receive(payload, timeout)
      parsed = protocol::parseTelemetryJson(payload)     // v1.13 문서 호환 스키마 파싱
      stats_.observe(parsed)                              // 패킷 시퀀스 통계
      telemetry_store.observe(parsed)                     // 프레임 매칭용 저장
      grid_map_tracker.observeMission(parsed.mission)     // 미션 종료 처리
      grid_map_tracker.observe(parsed.vision.grid_node)   // committed 노드 누적
      grid_map_tracker.observeDronePosition(...)          // 드론 위치 업데이트
      marker_tracker.observe(parsed.mission)              // 마커 발견 누적
```

메인 루프는 그냥 `telemetry_store` / `grid_map_tracker` / `marker_tracker`만
읽어가면 된다.

#### --- src/app/VideoReceiveWorker.cpp ---

```text
worker thread:
  while (running):
      frame = receiver_.receiveFrame(timeout)   // UdpMjpegReceiver가 청크 재조립
      stats_ 갱신
      if (frame):
          latest_frame_ = frame                  // 최신 한 장만 보관 (이전 건 덮어씀)
```

메인 루프는 `takeLatestFrame()` 한 번 호출로 최신 프레임을 꺼내간다. 못 따라간
프레임은 `overwritten_frames_`로 카운트만 된다.

---

### 주요 보조 모듈들의 역할 (메인 루프에서 참조하는 순서대로)

#### --- src/network/ ---

| 객체 | 역할 |
|---|---|
| `UdpTelemetryReceiver` | UDP 14550 소켓 오픈, 한 패킷씩 수신 |

#### --- src/video/ ---

| 객체 | 역할 |
|---|---|
| `VideoPacket` | `AQV1` 청크 헤더 파싱 |
| `JpegFrameReassembler` | 청크들을 모아서 완성된 JPEG 한 장으로 재조립 |
| `UdpMjpegReceiver` | UDP 5600 소켓에서 청크 수신 후 reassembler에 공급 |
| `GcsDiscoveryBeacon` | UDP 5601로 `AQGCS1 video_port=5600` broadcast (onboard가 GCS IP 자동 발견) |

#### --- src/protocol/ ---

| 객체 | 역할 |
|---|---|
| `TelemetryMessage` (`parseTelemetryJson`) | v1.13 문서 호환 텔레메트리 JSON을 구조체로 파싱. 알 수 없는 필드는 무시 |
| `TelemetryStats` | 시퀀스 번호 추적으로 drop/duplicate/out-of-order 패킷 카운트 |

#### --- src/telemetry/ ---

| 객체 | 역할 |
|---|---|
| `TelemetryStore` | 최근 텔레메트리들을 frame_id/timestamp로 인덱싱. 영상 프레임과 매칭에 사용 |
| `GridMapTracker` | onboard가 commit한 `vision.grid_node`만 누적하고 ASCII 격자맵으로 렌더 |
| `MarkerTracker` | mission 텔레메트리에서 발견된 마커 정보 누적 |
| `VisionLogFormatter` (`formatVisionLog`) | 텔레메트리 → 사람이 읽을 수 있는 로그 문자열 |
| `MarkerLogFormatter` | 마커 목록을 줄 단위로 포맷 |

#### --- src/overlay/ ---

| 객체 | 역할 |
|---|---|
| `OverlayPrimitive` | 백엔드 독립적인 오버레이 표현 (선/박스/텍스트 등) |
| `buildLineOverlays` | onboard 라인 metadata → magenta 선 + 추적점 |
| `buildIntersectionOverlays` | 교차점 분류/분기 ray → cyan 라벨 + yellow ray |
| `buildMarkerOverlays` | ArUco 마커 → 박스/모서리/중심/방향 표시 |

#### --- src/ui/ ---

| 객체 | 역할 |
|---|---|
| `VideoWindow` (`VideoWindow.cpp` = OpenCV / `VideoWindowWin32.cpp` = Win32+WIC) | JPEG 디코드 + overlay 그리기 + 창에 표시 |
| `VisionLogWindow` | 격자 맵 / 마커 / 디테일 로그를 세 패널로 출력. 창이 없으면 stdout fallback |

#### --- src/app/ ---

| 객체 | 역할 |
|---|---|
| `AstroquadGcsApp` | 모든 모듈을 조립하고 메인 UI 루프를 실행 (composition root) |
| `TelemetryWorker` | 텔레메트리 수신 스레드 + 파싱 + 저장소 갱신 |
| `VideoReceiveWorker` | 영상 수신 스레드 + 청크 재조립 + 최신 프레임 보관 |
| `VideoViewerApp` | `uav-gcs-video` 전용 (영상만 보는 단순 뷰어) |

---

## 정리: 한 패킷이 화면까지 흘러가는 길

```text
[Onboard]
   │
   ├── UDP 14550 telemetry JSON ──▶ TelemetryWorker
   │                                  → parseTelemetryJson
   │                                  → TelemetryStore / GridMapTracker / MarkerTracker
   │
   └── UDP 5600 MJPEG chunks ────▶ VideoReceiveWorker
                                      → UdpMjpegReceiver
                                      → JpegFrameReassembler
                                      → latest_frame_

                                          ▲
                                          │ 메인 루프가 둘을 합침
                                          │
[AstroquadGcsApp::run() 메인 루프]
   takeLatestFrame()
   findForFrame() → 같은 시점의 텔레메트리 매칭
   buildLine/Intersection/MarkerOverlays()
        ↓
   VideoWindow.showFrame(frame, overlays)        → [영상 창]
   VisionLogWindow.update(grid_text, markers, detail) → [로그 창]
```

이 순서가 머릿속에 들어오면, 각 모듈 헤더 파일을 펼쳐 세부를 읽기 시작해도
어디쯤에서 호출되는 코드인지 잃지 않을 수 있다.
