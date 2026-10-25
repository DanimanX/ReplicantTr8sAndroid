#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include "dsp_common.h"

namespace rt8 {

struct FirmwareTone {
    uint16_t id = 0;
    char name[24] = "Tone";
    uint8_t category = 1;
    uint8_t engineType = 1; // 1 = ACB, 2 = PCM/Sample, 3 = FM
};

struct FirmwareChannelPreset {
    uint16_t toneId = 0;
    uint8_t tune = 128;
    uint8_t decay = 128;
    uint8_t level = 150;
    uint8_t gain = 81;
    uint8_t pan = 128;
    uint8_t revSend = 0;
    uint8_t dlySend = 0;
    uint8_t ctrl = 128;
    uint8_t fxType = 0;
};

struct FirmwareKit {
    uint8_t number = 1;
    char name[20] = "INIT KIT";
    FirmwareChannelPreset channels[kNumChannels];
};

struct FirmwareVariation {
    uint8_t velocity[kNumTracks][kNumSteps] {};
    uint8_t flags[kNumTracks][kNumSteps] {};
};

struct FirmwarePattern {
    uint8_t number = 1;
    char code[8] = "1-01";
    char name[20] = "INIT PTN";
    uint16_t tempoTenths = 1280;
    uint8_t kitNumber = 1;
    FirmwareVariation variations[kNumVariations];
};

// Immutable once published. Engine keeps old banks alive so the audio thread can
// finish a block that is still pointing at the previous bank.
struct FirmwareBank {
    FirmwareTone tones[kNumTones] {};
    FirmwareKit kits[kNumKits] {};
    FirmwarePattern patterns[kNumPatterns] {};
    bool loaded = false;
    bool imported = false;
    char name[48] = "No firmware";
};

// Parses a raw rpg42_init_param.bin or a POSIX TAR (TR8S_UP.bin) image.
// The .zip container is unpacked by the Android layer before it gets here.
bool parseFirmwareBytes(const uint8_t* data, size_t size, FirmwareBank& bank, std::string& error);

struct ToneMetaInfo {
    const char* name = "";
    uint8_t category = 1;
    uint8_t engineType = 1;
};

// Fallback metadata for tone ids that a firmware image ships without a name.
bool toneMetaForId(int toneId, ToneMetaInfo& out);

// Synthesis category inferred from the tone id range for PCM / preset-synth
// banks that do not carry a category byte.
uint8_t inferToneCategory(int toneId);

} // namespace rt8
