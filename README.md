# uav-gcs

Ground control software for the Astroquad indoor/grid UAV search mission.

Operational documentation verified against the 2026-07-18 source/protocol
baseline. For the Korean real-flight handover and log-to-Codex workflow, see
[`../development-log/REAL_FLIGHT_ONBOARDING.md`](../development-log/REAL_FLIGHT_ONBOARDING.md).

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
- `logs/`: local development artifacts; the main GCS does not yet persist a
  complete flight log

## Executables

| Executable | Role |
|---|---|
| `astroquad-gcs` | Primary current GCS UI: telemetry + optional video + overlays + vision/grid log. **Use this for normal operation** — it is the only executable that draws line/intersection/marker overlays. |
| `uav-gcs-telem` | Telemetry-only console receiver / development probe. |
| `uav-gcs-video` | Raw MJPEG video receiver only — a transport smoke tool. It has no telemetry receiver, so it never draws overlays by design. |
| `mock_onboard`, `log_replayer` | Development tools. |

Shared modules stay in libraries so the main GCS can grow without copying
application wiring.

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

Main GCS:

```bash
./build/astroquad-gcs --config config
```

Telemetry-only receiver:

```bash
./build/uav-gcs-telem --config config
```

Windows:

```powershell
.\build\astroquad-gcs.exe --config config
.\build\uav-gcs-telem.exe --config config
.\build\uav-gcs-video.exe --config config
```

With a multi-config generator the executables may be under `build\Release`.

## Astroquad GCS

Use this for current onboard development:

```bash
./build/astroquad-gcs --config config
```

Onboard examples — the onboard default destination is already this laptop's
Tailscale address (`gcs-laptop` in the shared `KnownHosts.hpp` table), so
`--gcs-ip` is only needed for WSL/SITL or non-Tailscale setups:

```bash
# Raspberry Pi over Tailscale (no IP needed)
./build/vision_debug_node --config config --line-mode light_on_dark --video

# WSL/SITL (Windows host IP differs from the Tailscale address)
./build/line_follow_node --config config --target sitl --vision gazebo --line-mode dark_on_light --video --gcs-ip <gcs-ip>
./build/astroquad-onboard --config config --target sitl --vision gazebo \
  --world grid --line-mode dark_on_light --marker-count 4 \
  --video --gcs-ip <gcs-ip>
```

`--gcs-ip` accepts known names (`gcs-laptop`, `pi5`, `broadcast`) as well as
literal IPs. **Do not point onboard at its own IP** (`pi5`/`100.101.84.47`) —
that sends to the Pi itself and the laptop GCS receives nothing; omit
`--gcs-ip` to use the `gcs-laptop` default. The real runtime sends the
LTE-sized 600px/q55/12fps stream with XOR FEC and 6fps frame telemetry by
default; onboard `--fps <n>` overrides the video cap. Overlays stay aligned
because the GCS scales telemetry camera-space coordinates onto the received
frame.

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
`astroquad-onboard` intentionally resends the latest committed node every frame for UDP-loss
tolerance; GCS deduplicates it. `vision.drone_position` is parsed and stored,
but the current ASCII map renders the heading arrow at the latest committed
node rather than at a fractional sub-cell position.

`astroquad-onboard` also sends structured mission state, elapsed time, marker
progress, revisit status, and completion/landing results. The GCS displays these
for live observation. Post-flight truth remains the onboard per-run
`meta.json`, `events.jsonl`, and `frames.csv`; record the GCS/field view
separately because the main GCS has no complete persistent logger yet.

If the camera window says `waiting for video stream...` while logs update, the
onboard process is probably running telemetry-only. Add `--video` to enable raw
MJPEG debug video.

Video latency is not displayed. Pi/Windows clocks are not assumed synchronized,
and debug video is best-effort.

## Grid Arena SITL Workflow

PowerShell:

```powershell
cd astroquad\uav-gcs
.\build\astroquad-gcs.exe --config config
```

WSL:

```bash
bash ~/astroquad/uav-onboard/scripts/grid_arena_test.sh

WINDOWS_GCS_IP="$(ip route | awk '/default/ {print $3; exit}')"
cd ~/astroquad/uav-onboard
./build/astroquad-onboard --config config --target sitl --vision gazebo \
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

## Tailscale / Windows Firewall

The onboard Pi (`100.101.84.47`) reaches this GCS laptop (`100.85.239.73`)
over Tailscale. Windows commonly categorizes the Tailscale interface under
the **Public** firewall profile, so allow rules created earlier for a LAN
(Private profile) stop matching and every inbound UDP datagram is silently
dropped before it reaches the GCS socket — telemetry and video both show
nothing while the onboard side reports successful sends.

One-time setup (elevated PowerShell):

```powershell
powershell -ExecutionPolicy Bypass -File scripts\setup_windows_firewall.ps1
```

The script creates idempotent inbound allow rules for UDP 14550 (telemetry)
and UDP 5600 (video) across all profiles.

If packets still do not arrive:

- Verify the tunnel both ways: `tailscale ping <pi-tailscale-ip>` here and
  `tailscale ping <laptop-tailscale-ip>` on the Pi.
- Confirm the rules cover the active profile:
  `netsh advfirewall monitor show currentprofile` and
  `Get-NetFirewallRule -DisplayName "Astroquad GCS*"`.
- Watch the `[video-rx]` stats line from `uav-gcs-video`: `packets=0` means
  datagrams never reach the socket (firewall/routing); `packets>0` with
  `completed=0` means chunk loss or reassembly issues.
- `incomplete` growing much faster than `completed` means the path cannot
  carry the video bitrate (LTE-hotspot uplink, DERP relay). Check
  `tailscale ping` for `direct` vs `via DERP`, then run onboard with
  `--fps 6 --telemetry-fps 6` (see the uav-onboard README "Constrained
  links" recipe, ~1.2 Mbit/s total).
- As a last resort, capture on the Tailscale interface with `pktmon` or
  Wireshark to see whether datagrams arrive at the adapter at all.

## Troubleshooting

- If telemetry packets never arrive, check Windows Defender Firewall inbound
  UDP rules for `astroquad-gcs.exe` and `uav-gcs-telem.exe`, or run
  `scripts\setup_windows_firewall.ps1` (see the Tailscale section above).
- If video is missing but telemetry works, inspect onboard `[video]` counters.
  `video_sent=0`, `chunks_last=0`, and `last_bytes=0` mean video is disabled.
- If GCS discovery/broadcast is blocked, pass `--gcs-ip <laptop-ip>` to the
  onboard executable.
- If the grid map seems to skip nodes, check whether onboard is sending
  committed `vision.grid_node` events or only intersection candidates. GCS does
  not invent grid nodes.
