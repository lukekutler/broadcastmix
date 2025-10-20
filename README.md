# BroadcastMix v3

Early scaffolding for the BroadcastMix v3 application, following the 2025 specification.  
The project is organised around the major subsystems described in the spec and currently provides lightweight C++ stubs while the audio, UI, and plugin runtimes are integrated.

## Structure

- `src/audio` — audio graph primitives and engine controller stubs.
- `src/core` — application lifecycle and logging helpers.
- `src/plugins` — plugin host abstraction mirroring the sandbox runner design.
- `src/persistence` — project serialization entry points.
- `src/ui` — UI theming and node graph view placeholders.
- `src/control` — control surface discovery/management.
- `src/update` — Sparkle-based update service scaffold.
- `tests` — basic smoke test harness.
- `projects/SampleService.broadcastmix` — reference project bundle used for persistence tests.

## Building

```bash
./scripts/check_toolchain.sh
# macOS only: ensure Xcode Command Line Tools are installed
#   xcode-select --install
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build
```

To launch the JUCE shell app:

```bash
cmake --build build --target BroadcastMixApp
open build/apps/BroadcastMixApp_artefacts/BroadcastMix.app
```

JUCE is fetched from the upstream `develop` branch and compiled by default. Disable it with `-DBROADCASTMIX_FETCH_JUCE=OFF` if you just need the backend library or are targeting non-macOS environments.

### Continuous Integration

Pull requests are validated by `.github/workflows/build.yml`, which configures a Ninja build on Ubuntu and runs the smoke test suite.

## Next Steps

- Expose the JUCE AudioProcessorGraph topology in the node editor with live metering.
- Extend `.broadcastmix` persistence with snapshots, media references, and autosave recovery.
- Flesh out plugin runner IPC layer and connect to isolated processes.
- Connect control and update services to real hardware/APIs.

##

-Build using:
 ./run_app.sh