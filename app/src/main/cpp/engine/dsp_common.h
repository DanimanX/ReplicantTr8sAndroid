#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace rt8 {

constexpr int kNumChannels = 11;
constexpr int kNumSteps = 16;
constexpr int kNumVariations = 8;
constexpr int kNumKits = 128;
constexpr int kNumPatterns = 128;
constexpr int kNumTones = 522;
constexpr int kNumTracks = kNumChannels + 1; // 11 instruments + accent
constexpr int kMaxBlockFrames = 4096;

constexpr std::array<const char*, kNumChannels> kChannelNames = {
    "BD", "SD", "LT", "MT", "HT", "RS", "HC", "CH", "OH", "CC", "RC"
};

constexpr std::array<const char*, kNumChannels> kChannelFullNames = {
    "BASS DRUM", "SNARE DRUM", "LOW TOM", "MID TOM", "HIGH TOM",
    "RIM SHOT", "HAND CLAP", "CLOSED HI-HAT", "OPEN HI-HAT",
    "CRASH CYMBAL", "RIDE CYMBAL"
};

// GM percussion notes used by the hardware and by most controllers.
constexpr std::array<int, kNumChannels> kChannelNote = {
    36, 38, 43, 47, 50, 37, 39, 42, 46, 49, 51
};

inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

inline double clampd(double v, double lo, double hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

inline int clampi(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// Bit-level checks. They stay correct even when the translation unit is built
// with -ffast-math, where std::isfinite() may be folded away.
inline bool isFiniteSample(float v) {
    uint32_t bits = 0;
    std::memcpy(&bits, &v, sizeof(bits));
    return (bits & 0x7F800000u) != 0x7F800000u;
}

// Flush subnormal values to zero. Subnormals make the feedback paths (reverb
// combs, delay lines, filters) run orders of magnitude slower on ARM and are a
// classic source of intermittent audio dropouts.
inline float flushDenormal(float v) {
    uint32_t bits = 0;
    std::memcpy(&bits, &v, sizeof(bits));
    return ((bits & 0x7F800000u) == 0u) ? 0.0f : v;
}

inline float sanitize(float v) {
    return isFiniteSample(v) ? flushDenormal(v) : 0.0f;
}

struct StepCell {
    uint8_t velocity = 0;
    uint8_t flags = 0;
};

// MIDI note -> engine channel (GM drum map, chromatic C3..A3 and C1..A1).
inline int noteToChannel(int note) {
    switch (note) {
        case 35: case 36: return 0;
        case 38: case 40: return 1;
        case 41: case 43: return 2;
        case 45: case 47: return 3;
        case 48: case 50: return 4;
        case 37: case 75: return 5;
        case 39: case 54: return 6;
        case 42: case 44: return 7;
        case 46: case 55: return 8;
        case 49: case 52: case 57: return 9;
        case 51: case 53: case 56: case 59: return 10;
        default: break;
    }
    if (note >= 60 && note < 60 + kNumChannels) return note - 60;
    if (note >= 24 && note < 24 + kNumChannels) return note - 24;
    return -1;
}

enum ChannelParam : int {
    kTune = 0,
    kDecay = 1,
    kCtrl = 2,
    kLevel = 3,
    kPan = 4,
    kReverbSend = 5,
    kDelaySend = 6,
    kChannelParamCount = 7
};

enum MasterParam : int {
    kMasterVolume = 0,
    kMasterDrive = 1,
    kMasterReverb = 2,
    kMasterDelay = 3,
    kMasterFilter = 4,
    kScatterDepth = 5,
    kMasterParamCount = 6
};

} // namespace rt8
