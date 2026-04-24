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
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

## Run

```bash
./uav_gcs --config ../config
```
