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

## Pi Bring-Up Order

1. Build and start this GCS receiver on the laptop.
2. Build and start `uav_gcs_video` on the laptop if camera video is needed.
3. Run `uav-onboard/build/video_streamer --source rpicam --config config` on the Pi.
   It should print `discovered GCS video receiver at <ip>:5600`.
4. Run `uav-onboard/build/uav_onboard --config config --count 10` on the Pi.
5. Confirm this GCS prints `TELEMETRY` packets with increasing `seq` values.

The onboard default sends telemetry/video to IPv4 broadcast
`255.255.255.255`, so the laptop IP usually does not need to be edited. If the
network blocks discovery or broadcast, override the video destination with
`--gcs-ip <laptop-ip>`.
