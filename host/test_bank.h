#pragma once

// Synthetic, neutral bank used only by the host tests. The shipping library no
// longer contains any built-in kit or pattern content: the Android engine stays
// locked until the user imports their own firmware image.

#include <cstdio>

#include "firmware.h"

namespace rt8 {

inline FirmwareBank makeTestBank() {
    FirmwareBank bank;

    for (int i = 0; i < kNumTones; ++i) {
        auto& tone = bank.tones[i];
        tone.id = static_cast<uint16_t>(i);
        tone.category = static_cast<uint8_t>((i % 11) + 1);
        tone.engineType = 1;
        std::snprintf(tone.name, sizeof(tone.name), "Tone %03d", i);
    }

    constexpr uint16_t kToneIds[kNumChannels] = {0, 4, 6, 7, 8, 15, 17, 21, 22, 23, 25};
    constexpr uint8_t kRev[kNumChannels] = {12, 55, 32, 32, 32, 30, 45, 18, 34, 60, 60};
    constexpr uint8_t kDly[kNumChannels] = {6, 26, 18, 18, 18, 25, 30, 10, 28, 55, 55};

    for (int k = 0; k < kNumKits; ++k) {
        auto& kit = bank.kits[k];
        kit.number = static_cast<uint8_t>(k + 1);
        std::snprintf(kit.name, sizeof(kit.name), "Test Kit %03d", k + 1);
        for (int ch = 0; ch < kNumChannels; ++ch) {
            auto& cp = kit.channels[ch];
            cp.toneId = kToneIds[ch];
            cp.tune = 128;
            cp.decay = 128;
            cp.level = 150;
            cp.gain = 81;
            cp.pan = 128;
            cp.revSend = kRev[ch];
            cp.dlySend = kDly[ch];
            cp.ctrl = 128;
        }
    }

    for (int i = 0; i < kNumPatterns; ++i) {
        auto& p = bank.patterns[i];
        p.number = static_cast<uint8_t>(i + 1);
        std::snprintf(p.code, sizeof(p.code), "%d-%02d", i / 16 + 1, i % 16 + 1);
        std::snprintf(p.name, sizeof(p.name), "Test PTN %d-%02d", i / 16 + 1, i % 16 + 1);
        p.tempoTenths = 1280;
        p.kitNumber = static_cast<uint8_t>((i % kNumKits) + 1);

        for (int s = 0; s < kNumSteps; s += 4) {
            p.variations[0].velocity[0][s] = 112;  // BD
            p.variations[0].velocity[11][s] = 100; // RC
        }
        p.variations[0].velocity[1][4] = 104;  // SD
        p.variations[0].velocity[1][12] = 104;
        for (int s = 0; s < kNumSteps; s += 2) p.variations[0].velocity[7][s] = 80;
        p.variations[0].velocity[8][2] = 88;
        p.variations[0].velocity[8][6] = 88;
        p.variations[0].velocity[9][13] = 80;
    }

    bank.loaded = true;
    bank.imported = true;
    std::snprintf(bank.name, sizeof(bank.name), "Test firmware");
    return bank;
}

} // namespace rt8
