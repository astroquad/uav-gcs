# uav-gcs

Ground control software for the indoor UAV search mission.

This project owns telemetry display, mission monitoring, operator commands,
video display, and GCS-side logging.

## Layout

- `config/`: runtime TOML configuration
- `src/`: GCS application source
- `tools/`: mock onboard and log replay utilities
- `test_data/`: captured telemetry for repeatable tests
- `tests/`: unit tests
- `scripts/`: build and run helpers
- `docs/`: design notes and protocol reference
- `third_party/`: vendored dependencies when needed
- `logs/`: runtime logs

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Windows PowerShell

With the default Visual Studio CMake generator:

```powershell
cmake -S . -B build
cmake --build build --config Release
```

With Ninja:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Run Telemetry Receiver

```bash
./build/uav_gcs --config config
```

On Windows with a multi-config generator, the executable may be under
`build/Release/uav_gcs.exe` or `build/Debug/uav_gcs.exe`.

```powershell
.\build\Release\uav_gcs.exe --config config
```

If using Ninja on Windows:

```powershell
.\build\uav_gcs.exe --config config
```

## Run Video Receiver

`uav_gcs_video` uses OpenCV with `core`, `imgcodecs`, `highgui`, and `imgproc`
when OpenCV is available to CMake. On Windows, if OpenCV is not installed, the
build uses a Win32/WIC fallback video window so the target is still built.

```bash
./build/uav_gcs_video --config config
```

On Windows with a multi-config generator:

```powershell
.\build\Release\uav_gcs_video.exe --config config
```

If using Ninja on Windows:

```powershell
.\build\uav_gcs_video.exe --config config
```

Start this before running `uav-onboard/build/video_streamer` on the Raspberry Pi.
While this video receiver is running, it broadcasts a small discovery beacon.
With the onboard default config, `video_streamer` uses that beacon to discover
the laptop IP and then sends video by unicast.

## Run Vision Debug Receiver

Use this for the current vision development stage. It receives the raw MJPEG
camera stream and vision telemetry at the same time, draws marker and line
overlays on the GCS side, and opens a separate vision log window when the
platform supports it. The vision receiver drains UDP video packets on a
background receive thread and the UI displays only the latest complete JPEG
frame, so OpenCV/Win32 drawing does not block socket reads.

```bash
./build/uav_gcs_vision_debug --config config
```

On Windows with Ninja:

```powershell
.\build\uav_gcs_vision_debug.exe --config config
```

Start this before running `uav-onboard/build/vision_debug_node` on the
Raspberry Pi. The onboard stream remains raw camera JPEG; marker boxes, labels,
direction arrows, magenta line contours, and green line tracking points are
drawn only by GCS.

Expected live overlay behavior:

- ArUco markers: marker box, corner points, center point, direction arrow, label.
- Line tracing: magenta connected line contour/border and green tracking point.
  Cross-shaped intersections are shown when they are part of the selected
  contour.
- If both detectors are enabled, both overlays are shown in the same video
  window using the same frame sequence synchronization.
- The separate vision log window shows packet stats, marker state, line state,
  detector latency, onboard read/decode/JSON/send/video timing, line contour
  workload counters, video queue drops/skips/failures, video chunk counts, GCS
  completed/incomplete frame counts, displayed FPS, Pi board/OS/load/memory/
  throttling/Wi-Fi state, IMX519 camera focus/exposure settings, capture/
  processing FPS, optional Pi CPU temperature, and raw-vs-filtered line state.
  If the log window backend is unavailable, the same text is printed to the
  terminal.

Raspberry Pi 4 + IMX519-78 can produce larger MJPEG frames than the previous
Zero-class camera setup. GCS still treats video as best-effort debug data:
complete/incomplete frame counts and displayed FPS are the useful health
signals, while actual mission decisions must come from onboard telemetry.

If the Pi discovers the GCS IP but this app still shows no telemetry packets,
check Windows Defender Firewall. `uav_gcs_vision_debug.exe` needs inbound UDP
allow rules for the current network profile, the same as `uav_gcs.exe` and
`uav_gcs_video.exe`.

Useful test options:

```powershell
.\build\uav_gcs_vision_debug.exe --config config
.\build\uav_gcs_vision_debug.exe --config config --marker-log-ms 1000
```

## Local Mock Test

Start the receiver in one terminal:

```bash
./build/uav_gcs --config config --count 5
```

Send mock onboard telemetry in another terminal:

```bash
./build/mock_onboard --gcs-ip 127.0.0.1 --count 5
```

On Windows PowerShell with the default Visual Studio CMake generator:

```powershell
.\build\Release\uav_gcs.exe --config config --count 5
.\build\Release\mock_onboard.exe --gcs-ip 127.0.0.1 --count 5
```

## Tests

Configure with tests enabled:

```powershell
cmake -S . -B build-tests -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

Current focused tests cover telemetry parsing for `vision.line` and GCS line
overlay primitive generation. Video reassembly stats are also covered so frame
drop diagnostics do not regress silently.

## Pi Bring-Up Order

1. Build and start this GCS receiver on the laptop.
2. For metadata-only onboard runs, start the Pi with no `--video` flag:
   `./build/vision_debug_node --config config --line-only --line-mode light_on_dark`.
3. If the GCS camera window should show raw camera video and overlays, add
   `--video` on the Pi:
   `./build/vision_debug_node --config config --line-only --line-mode light_on_dark --video`.
4. Confirm this GCS prints `TELEMETRY` packets with increasing `seq` values.

The onboard default sends telemetry to IPv4 broadcast `255.255.255.255`. When
debug video is enabled with `--video`, the video destination is also discovered
or sent by broadcast, so the laptop IP usually does not need to be edited. If
the network blocks discovery or broadcast, override the video destination with
`--gcs-ip <laptop-ip>`.

If the camera window stays on `waiting for video stream...` while the vision log
continues to update, inspect the `[video]` counters in the log. `video_sent=0`,
`chunks_last=0`, and `last_bytes=0` mean the Pi is intentionally running in
telemetry-only mode; restart the Pi command with `--video`.

The camera overlay intentionally does not show video latency. The debug video
path is best-effort, and Pi/Windows clocks are not assumed to be synchronized,
so latency estimates from raw capture timestamps are misleading.
