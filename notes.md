# Session Notes — BroadcastMix v3

## 2025-10-06
- Reviewed `specifications/BroadcastMix v3 Specification.md` and UI direction to align scaffolding.
- Created initial CMake-based C++ project structure matching core modules (audio, plugins, persistence, UI, control, update).
- Added stub implementations with logging hooks and TODO markers for future JUCE integration.
- Attempted to configure project with `cmake -S . -B build`; command unavailable in environment (missing CMake binary).
- Roadmap drafted to track phase progression; current milestone marked as Phase 0 foundations.
- Added `scripts/check_toolchain.sh` for local dependency checks and `.github/workflows/build.yml` for CI coverage.
- After installing CMake/Ninja, local configure now fails while building JUCE helper (`algorithm` header missing); need macOS Command Line Tools (`xcode-select --install`).
- Switched JUCE fetch behind `BROADCASTMIX_FETCH_JUCE` flag to unblock builds on macOS 15 until compatible JUCE release is pinned; project now builds/tests locally with `cmake --build build` + `ctest --test-dir build`.
- Upgraded to JUCE `develop`, added `apps/BroadcastMixApp` JUCE shell (DocumentWindow + sanctuary-inspired theme), and defaulted FetchContent to ON.
- Implemented JUCE-backed audio engine: default Broadcast→Mix topology, -3 dB mix trim gain stage, lazy audio-device setup, and pass-through processors for channel/group/utility nodes.
- Defined `.broadcastmix` project layout with JSON graph persistence, sample project bundle, and serializer read/write logic (fallbacks when JSON missing).
- Snapshot index + autosave metadata now persist alongside graphs; sample project includes default snapshot and autosave graph for recovery testing.
