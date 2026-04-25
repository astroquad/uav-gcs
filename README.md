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

## Run Telemetry Receiver

```bash
./build/uav_gcs --config config
```

On Windows with a multi-config generator, the executable may be under
`build/Release/uav_gcs.exe` or `build/Debug/uav_gcs.exe`.

## Local Mock Test

Start the receiver in one terminal:

```bash
./build/uav_gcs --config config --count 5
```

Send mock onboard telemetry in another terminal:

```bash
./build/mock_onboard --gcs-ip 127.0.0.1 --count 5
```

## Pi Bring-Up Order

1. Build and start this GCS receiver on the laptop.
2. Find the laptop IP address on the same Wi-Fi network.
3. Set the Raspberry Pi `uav-onboard/config/network.toml` `[gcs].ip` to that IP.
4. Run `uav-onboard/build/camera_preview` on the Pi to validate the camera.
5. Run `uav-onboard/build/uav_onboard --config config --count 10` on the Pi.
6. Confirm this GCS prints `TELEMETRY` packets with increasing `seq` values.
