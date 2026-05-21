# uav-gcs

Ground control software for the Astroquad indoor/grid UAV search mission.

The current GCS is an observation and tuning tool: it receives onboard
telemetry, optional raw MJPEG debug video, draws overlays from onboard metadata,
and renders vision/grid logs. It does not run ArUco, line, intersection, or
mission decision logic locally.

## Layout

- `config/`: runtime TOML configuration
- `src/`: GCS application source
- `tools/`: mock onboard and log replay utilities
- `tests/`: unit tests
- `docs/`: protocol reference
- `logs/`: runtime logs

## Executables

| Executable | Role |
|---|---|
| `uav_gcs` | Basic telemetry receiver; final mission dashboard composition root target. |
| `uav_gcs_vision_debug` | Primary current UI: telemetry + optional video + overlays + vision/grid log. |
| `uav_gcs_video` | Raw MJPEG video receiver only. |
| `mock_onboard`, `log_replayer` | Development tools. |

Shared modules stay in libraries so `uav_gcs` can grow into the final dashboard
without copying `VisionDebugApp`.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Windows PowerShell with Visual Studio generator:

```powershell
cmake -S . -B build
cmake --build build --config Release
```

Windows PowerShell with Ninja:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

If OpenCV is unavailable on Windows, the build uses the Win32/WIC fallback
window backend for video decode/drawing.

## Run

Telemetry-only receiver:

```bash
./build/uav_gcs --config config
```

Windows:

```powershell
.\build\uav_gcs.exe --config config
.\build\uav_gcs_vision_debug.exe --config config
.\build\uav_gcs_video.exe --config config
```

With a multi-config generator the executables may be under `build\Release`.

## Vision Debug Receiver

Use this for current onboard development:

```bash
./build/uav_gcs_vision_debug --config config
```

Onboard examples:

```bash
./build/vision_debug_node --config config --line-only --line-mode dark_on_light --video
./build/line_follow_node --config config --target sitl --vision gazebo --line-mode dark_on_light --video --gcs-ip <gcs-ip>
./build/grid_mission_node --config config --target sitl --vision gazebo \
  --world grid --line-mode dark_on_light --marker-count 4 \
  --video --gcs-ip <gcs-ip>
```

Expected overlays:

- ArUco marker box, corners, center, direction arrow, label.
- Line contour in magenta.
- Current line tracking X as a red point at camera-center Y with a green
  horizontal error line.
- Compact cyan intersection type and yellow present-branch rays.

Expected log groups:

- Packet/video receive stats.
- Camera/system timing and Pi health.
- Line raw/filtered/held/rejected state and detector workload.
- Intersection raw/stabilized state and branch scores.
- `intersection_decision` sliding-window branch evidence.
- `grid_node` local coordinate events and `[grid-map]` ASCII map.

The GCS grid map consumes `vision.grid_node` only after onboard commits a node.
`grid_mission_node` intentionally resends the latest committed node every frame
for UDP-loss tolerance; GCS deduplicates it. `vision.drone_position` is parsed
and stored, but the current ASCII map renders the heading arrow at the latest
committed node rather than at a fractional sub-cell position.

If the camera window says `waiting for video stream...` while logs update, the
onboard process is probably running telemetry-only. Add `--video` to enable raw
MJPEG debug video.

Video latency is not displayed. Pi/Windows clocks are not assumed synchronized,
and debug video is best-effort.

## Grid Arena SITL Workflow

PowerShell:

```powershell
cd astroquad\uav-gcs
.\build\uav_gcs_vision_debug.exe --config config
```

WSL:

```bash
bash ~/astroquad/uav-onboard/scripts/grid_arena_test.sh

WINDOWS_GCS_IP="$(ip route | awk '/default/ {print $3; exit}')"
cd ~/astroquad/uav-onboard
./build/grid_mission_node --config config --target sitl --vision gazebo \
  --world grid --line-mode dark_on_light --marker-count 4 \
  --video --gcs-ip "$WINDOWS_GCS_IP"
```

Watch the lower log pane for `[intersection-decision]` and `[grid-node]`; the
fixed top pane renders `[grid-map]`.

## Tests

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Current tests cover:

- Telemetry parsing for line, intersection, intersection decision, and grid
  node.
- MJPEG chunk reassembly/drop diagnostics.
- Line and intersection overlay primitive generation.
- Grid map tracker dedup/edge rendering.

## Troubleshooting

- If telemetry packets never arrive, check Windows Defender Firewall inbound
  UDP rules for `uav_gcs.exe` and `uav_gcs_vision_debug.exe`.
- If video is missing but telemetry works, inspect onboard `[video]` counters.
  `video_sent=0`, `chunks_last=0`, and `last_bytes=0` mean video is disabled.
- If GCS discovery/broadcast is blocked, pass `--gcs-ip <laptop-ip>` to the
  onboard executable.
- If the grid map seems to skip nodes, check whether onboard is sending
  committed `vision.grid_node` events or only intersection candidates. GCS does
  not invent grid nodes.
