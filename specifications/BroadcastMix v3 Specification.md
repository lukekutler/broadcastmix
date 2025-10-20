# BroadcastMix v3 Specification — 2025
_Version: 3.0 | Date: 2025-10-06 16:48:18_

---

## 1. Overview

**BroadcastMix** is an integrated live broadcast mixing application designed for church livestreams.  
It bridges the gap between DAW-level precision and live console reliability, providing a predictable, volunteer-friendly environment for broadcast engineers.

The philosophy:  
> *“Engineers mix during the week, volunteers run it on Sunday, and everything sounds the same.”*

---

## 2. System Architecture

### 2.1 Core Modules
- **Audio Engine (C++/JUCE)** — low-latency graph engine managing nodes, buses, and plugin I/O.
- **Plugin Host (Runner Processes)** — sandboxed subprocesses for AU/VST3 plugins.
- **UI Layer (JUCE/Swift)** — node graph editor, meters, routing inspector.
- **Persistence Engine** — manages `.broadcastmix` projects, autosave, crash recovery.
- **Control Interface** — Stream Deck SDK, MIDI, and keyboard mappings.
- **Update Service** — Sparkle 2 integration for macOS update distribution.

### 2.2 Threading Model
```
Main Thread —> UI + Message Dispatch
Audio Thread —> Realtime audio processing (graph engine)
Runner Threads —> Plugin isolation subprocesses (inter-process)
Autosave Thread —> Periodic serialization, 5min default interval
```
All inter-thread communication uses lock-free queues. Audio thread is never blocked.

---

## 3. Signal Flow & Bus Topology

### 3.1 Core Graph Layout
```
[Inputs] → [Channel Processing] → [Group Buses] → [Broadcast Bus] → [Broadcast Master Chain] 
                                                                 ↓
                                                             [Mix Bus] → [Trim -3dB] → [+Utility Channels] → [Output/Print]
```

### 3.2 Bus Definitions

| Bus | Purpose | Processing Shared? | Output | Notes |
|------|----------|--------------------|---------|--------|
| **Broadcast Bus** | Primary program mix for livestream | Yes (shared with Mix Bus) | Livestream feed | Includes master chain processing |
| **Mix Bus** | Engineer monitoring + print source | Shared with Broadcast Bus | Local monitoring / record | Adds utility channels after -3dB trim |
| **Utility Channels** | Elements like talkback, host mic, click ref | Not routed to Broadcast | Summed into Mix Bus only | Hidden from Broadcast context |

### 3.3 Mix Bus Definition
```
Mix Bus = (Broadcast Bus post-master) → −3 dB headroom trim → + Utility Channels → Output
```
- Tap point: **post-Broadcast master chain**
- Apply **−3 dB** attenuation
- Sum in utility sources
- Feed both monitor and print output

---

## 4. Session & Project Management

### 4.1 File Format
`.broadcastmix` is a directory-based project containing:
```
ProjectName.broadcastmix/
  ├── graph.json
  ├── snapshots/
  ├── media/
  ├── captures/
  ├── autosave/
  └── logs/
```

### 4.2 Autosave & Recovery
- Background autosave every **5 minutes**.
- On crash or force quit, reopening BroadcastMix will prompt recovery from the autosave version.


### 4.3 Storage Policy
- Only **local and external volumes** supported.
- Network/NAS storage is ignored to prevent latency or corruption.

---

## 5. Modes & State Machine

Two operational modes:

| Mode | Description | Behavior |
|-------|--------------|-----------|
| **Record** | Multitrack capture and live monitoring | Arms all recordable channels, enables capture folder |
| **Live** | Active broadcast mix with snapshot recall and meters | Disables record engine, focuses on live processing |


---

## 6. Plugin Host & Sandbox Model

### 6.1 Out-of-Process Runners
Each plugin instance runs in its own subprocess, launched via IPC.

| Feature | Behavior |
|----------|-----------|
| Isolation | Plugin crash never affects host |
| Communication | Shared memory ring buffer + OSC-style IPC |
| Persistence | Plugin process persists across snapshot recalls |
| Crash Handling | Auto-bypass, badge, retry on next load |

### 6.2 Lifecycle Pseudo-code
```python
on_snapshot_recall():
    for plugin in active_plugins:
        plugin.update_params(snapshot.params[plugin.id])
        # plugin process stays alive; audio uninterrupted
```

### 6.3 Supported Formats
- **AUv2 / AUv3**
- **VST3**
- No AAX support in v3.

---

## 7. UI Layer

### 7.1 Node Graph
- Multi-select and drag supported.
- Auto-highlight compatible ports during cable drag.
- Auto-wire upstream→downstream when deleting nodes.
- Each node includes a small **audio meter** (post-output) and overload indicator.

### 7.2 Metering
- **Peak + Hold (300ms)**; red over clears on click.
- **True-peak oversampling: 4×**.
- **Global CPU meter** in status bar.

### 7.3 Keyboard Map (Fixed)
| Action | Shortcut |
|---------|-----------|
| Snapshot Next | ⌘→ |
| Snapshot Prev | ⌘← |
| Record Toggle | ⌘R |
| Marker Drop | ⌘M |

---

## 8. Snapshots & Recall System

### 8.1 Snapshot Structure
Snapshots store serialized plugin and fader parameters.

```json
{
  "snapshot_name": "How Great Is Our God",
  "timestamp": "2025-10-06T11:00:00Z",
  "parameters": {
    "plugin_123": {"threshold": -6.2, "attack": 3.5},
    "channel_07": {"fader": -3.0, "pan": 0.25}
  }
}
```

### 8.2 Safe Channels
Channels flagged as **“Never change on recall”** (e.g., host/pastor mics).  
Defined on channel strip.

### 8.3 Glide Timing
Snapshot transition times are defined as S/M/L (default 3s), applied per recall.

---

## 9. Recording & Export Engine

### 9.1 Capture
- Offline (faster-than-realtime) render.
- All captures recorded at **48kHz / 24-bit WAV**.
- File naming convention: `01_Kick.wav`, `02_Snare.wav`, etc.

### 9.2 Stem Export
- Stems are **pre-bus (raw)** — excludes group processing.
- Utility tracks are also included on all stems

### 9.3 Print Export
- Always prints **Mix Bus output** (post-trim, post-utility).  
- Export options:
  - 24-bit WAV (default)
  - 16-bit WAV + TPDF dither (optional)

### 9.4 File Naming
```
<ServiceName>_MixPrint.wav
<ServiceName>_Stems/<TrackName>.wav
```

---

## 10. Control Surfaces & External APIs

### 10.1 Stream Deck Integration
Default actions:
- Next Snapshot
- Previous Snapshot
- Recall Snapshot 1–5
- Drop Marker
- Dim Mix Bus
- Start/Stop Record

### 10.2 MIDI / OSC (Future)
Stub endpoints available:
```
/snapshot/next
/snapshot/prev
/record/toggle
```
No external protocol control in v3.

---

## 11. Error Handling & Crash Recovery

| Event | Response |
|--------|-----------|
| Plugin crash | Auto-bypass, badge, log entry, retry next load |
| Graph error | Attempt reconnect, warn user |
| Disk full | Pause capture, prompt user |
| Application crash | Auto-reopen last autosave project |

---

## 12. Build & Update Delivery

### 12.1 Update Channel
- **Phase 1:** Manual internal distribution (notarized .dmg)
- **Phase 2:** Sparkle 2 private beta with Ed25519 signing
- **Phase 3:** Public channel + Preferences → Updates

### 12.2 Sparkle Implementation Notes
- Hardened runtime + notarization required.
- Appcast served via HTTPS.
- Delta updates enabled.
- Ed25519 public key embedded in app.

---

## 13. QA Matrix & Compatibility

| Category | Target |
|-----------|--------|
| **macOS** | 13.0+ |
| **CPU** | Intel (priority), Apple Silicon tested |
| **Plugin Formats** | AUv2, AUv3, VST3 |
| **Max I/O** | 64×64 |
| **Audio Interfaces** | Avid HD Native, UAD Apollo, Focusrite Scarlett 18i20 |

---

## 14. Future Extensions
- LUFS loudness UI (stubbed, disabled)
- Custom keyboard map editor
- Network/NAS project storage
- Rehearse mode reintroduction
- Multi-user project locking

---

**End of Specification**
