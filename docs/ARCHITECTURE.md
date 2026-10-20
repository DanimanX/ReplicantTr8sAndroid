# Architecture

## Signal flow

```
USB MIDI in ──► MidiController ──► Engine::pushMidiMessage ─┐
Tap / pad ────► Engine::trigger ────────────────────────────┤
                                                           ▼
Pattern grid (8 × 12 × 16 atomics) ──► step scheduler ──► 11 voices
                                                           │
                                     per-channel sends ────┤
                                                           ▼
                                        master bus (delay · reverb · filter · drive)
                                                           │
                                                           ▼
                                               Oboe float32 stereo out
                                                           │
                                      scope ring · meters · MIDI out ring
```

## Threads

| Thread | Owns | Notes |
| --- | --- | --- |
| Oboe audio callback | voices, master bus, sequencer counters, scope ring | No allocations, no locks, no JNI |
| MIDI callback / JNI producers | MIDI-in ring | Mutex protects multiple producers; audio thread pops without locking |
| Engine polling coroutine | UI snapshots | Reads atomics and immutable firmware banks |
| Kotlin MIDI output pump | `MidiInputPort.send` | Drains the engine's MIDI-out ring every ~2 ms |
| Compose main thread | rendering, gestures | Only calls atomic setters / command JNI |

## State ownership

- **Audio-owned:** voice envelopes, filters, playhead, clock counters, scope ring, peak meters.
- **Atomic shared:** channel parameters, tone IDs, mute/solo, master parameters, tempo, shuffle, accent, variation, fill, pattern cells.
- **Immutable shared:** firmware banks. A new bank is parsed into a fresh allocation and published with an atomic pointer swap. Old banks are kept alive so an in-flight audio block can finish safely.
- **UI-owned:** navigation, dialogs, text fields, local knob drag state.

`EngineUiState` is rebuilt roughly 30 times per second from JNI snapshots. Meters and scope are separate `StateFlow`s so only the components that need them recompose at that rate.

## Sequencer

- 16 steps at 1/16 note, 8 variations, 12 tracks (11 instruments + accent).
- Each pattern cell is packed as `(flags << 8) | velocity` in `std::atomic<uint16_t>`.
- Step length is `(60 / BPM) × 0.25 × sampleRate`, with alternating-step swing.
- Fill-in uses variation H and adds a snare/tom roll in the last four steps.
- Mute/solo are applied at mix time; voices still run so envelopes decay naturally.

## Synthesis

`AcbVoice` is a JUCE-free port of the original ReplicantTR8S voice. The engine selects a model from the firmware tone ID:

- 0–25 / 81–83 / 158: 808 ACB models
- 26–41 / 84–85 / 157: 909 ACB models
- 42–56: 707/626 12-bit-style models
- 57–71: 727 Latin percussion
- 72–80: 606 models
- 86–156: 2-operator FM
- 159–173: CR-78
- 181–354: legacy PCM-style
- 355–521: synth/PCM-style presets

The master bus is a stereo delay, Freeverb-style reverb, state-variable master filter, tanh drive and limiter.

## Firmware bank

`FirmwareBank` contains:

- `FirmwareTone[522]`
- `FirmwareKit[128]`
- `FirmwarePattern[128]` (8 variations × 12 tracks × 16 steps)

The Android layer unzips the user's `.zip` with `java.util.zip`, then passes the contained `.bin` to native code. Native code handles:

1. POSIX TAR member lookup (`TR8S_UP.bin`).
2. LZSS decompression of `rpg42_init_param.bin`.
3. Fixed-offset parsing of TONE, KIT and PTN chunks.
4. Bank publication and reload of the current kit/pattern.

No firmware-derived data is compiled into the binary. The built-in preview bank is original clean-room content.

## MIDI

- MIDI 1.0 byte stream: `MidiStreamParser` handles running status, realtime interleaving and SysEx skipping.
- MIDI 2.0 UMP: `UmpCodec` decodes MT=1 system realtime, MT=2 MIDI 1.0 channel voice and MT=4 MIDI 2.0 channel voice; encodes outgoing notes as 16-bit-velocity MIDI 2.0 and clock as UMP system realtime.
- Incoming messages are sample-scheduled against the audio callback timestamp.
- Outgoing events carry an audio-clock timestamp, offset by the stream's output latency, and are scheduled through `MidiInputPort.send`.
- Clock: internal generation at 24 PPQN; external clock mode advances the sequencer every 6 pulses (one 16th) and estimates BPM from recent tick intervals.

## Low-latency choices

- Oboe `LowLatency` performance mode.
- AAudio exclusive mode attempted at 48 kHz.
- One-burst buffer target, with shared-mode fallback.
- Float32 stereo, no sample-rate conversion in the engine.
- Fixed-size event rings; no audio-thread allocation.
- `-O3 -ffast-math` for the native engine.
- Foreground service + audio focus + partial wake lock.
- UI reports the real API, sharing mode, buffer size and latency.

## Known limits

- USB MIDI hardware was not available in the verification environment; parser/UMP/clock behavior is unit-tested, but real-device validation is recommended.
- MIDI 2.0 UMP availability and packet transport depend on the Android version and the USB device's advertised protocol.
- Phone builds are portrait-only; tablets support portrait and landscape.
- User sample import is not implemented in this release.
