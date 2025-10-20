# BroadcastMix v3 Roadmap

## Phase 0 – Foundations (In Progress)
- [x] Establish CMake project with module-aligned library skeleton.
- [x] Stub core subsystems (audio engine, plugin host, persistence, UI, control, update).
- [x] Provide toolchain bootstrap script and CI build workflow.
- [x] Integrate JUCE framework and bootstrap GUI shell.
- [x] Implement real-time audio graph within JUCE environment.
- [x] Define `.broadcastmix` project schema and serializer IO (graph + snapshots + autosave metadata).
- [ ] Add structured logging sink rotation and on-disk persistence.
- [ ] Surface graph topology & snapshot browser inside JUCE shell for visual verification.
- [ ] Persist detailed snapshot payloads (per-node params) and media references.
- [ ] Autosave recovery flow with user prompt and telemetry event logging.

## Phase 1 – Audio Graph & Processing
- [ ] Implement channel strip processing chain with per-node metering.
- [ ] Model Broadcast, Mix, and Utility bus routing including -3 dB trim.
- [ ] Introduce snapshot-aware parameter automation with glide timing.
- [ ] Connect plugin runner processes with sandbox IPC.

## Phase 2 – UI Layer
- [ ] Build node graph editor with live metering and drag interactions.
- [ ] Implement dark sanctuary UI theme with responsive lighting cues.
- [ ] Add snapshot browser and safe-channel indicators.
- [ ] Provide CPU meter and status bar diagnostics.

## Phase 3 – Session Management
- [ ] Complete autosave, crash recovery, and open-on-relaunch flows.
- [ ] Enforce storage policy (local/external) and disk space monitoring.
- [ ] Implement Record vs Live mode state machine and UI affordances.

## Phase 4 – Capture & Export
- [ ] Build multitrack capture pipeline (48kHz/24-bit WAV).
- [ ] Implement stem and print export workflows with file naming conventions.
- [ ] Add offline render engine for faster-than-realtime bounce.

## Phase 5 – Control & Updates
- [ ] Integrate Stream Deck SDK actions and MIDI stubs.
- [ ] Build Sparkle 2 update distribution path (private beta → public).
- [ ] Add telemetry for plugin crash handling and retry logic.


## Completed Milestones
- 2025-10-06: Specification review and initial code scaffold committed.
- 2025-10-06: Toolchain bootstrap script and GitHub Actions build workflow added.
- 2025-10-06: macOS build verified locally (CMake + Ninja + smoke test) with optional JUCE integration gated.
- 2025-10-06: JUCE develop branch integrated; BroadcastMix shell app launched.
- 2025-10-06: AudioProcessorGraph bridge created for spec'd Broadcast → Mix topology with -3 dB trim node.
- 2025-10-06: `.broadcastmix` persistence (graph, snapshot index, autosave metadata) implemented with sample bundle.

## Session Log
- 2025-10-06 (Evening Pause): Paused after wiring persistence; next session should tackle Phase 0 to-do items above (logging sink, JUCE UI surfacing, snapshot payload details, autosave UX).
- Earlier milestones are listed chronologically above.
