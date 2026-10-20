#include "firmware.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace rt8 {
namespace {

struct ToneMeta {
    int id;
    const char* name;
    int category;
    int engine;
};

const ToneMeta kToneMeta[] = {
    {0, "808 Bass 1", 1, 1},
    {1, "808 Bass 2", 1, 1},
    {2, "808 Long Bass", 1, 1},
    {4, "808 Snare 1", 2, 1},
    {5, "808 Snare 2", 2, 1},
    {6, "808 Low Tom", 3, 1},
    {7, "808 Mid Tom", 3, 1},
    {8, "808 High Tom", 3, 1},
    {15, "808 Rim Shot", 4, 1},
    {17, "808 Hand Clap", 5, 1},
    {21, "808 Closed HH", 6, 1},
    {22, "808 Open HH", 6, 1},
    {23, "808 Crash Cym", 7, 1},
    {25, "808 Cowbell", 11, 1},
    {26, "909 Bass 1", 1, 1},
    {27, "909 Bass 2", 1, 1},
    {28, "909 Snare 1", 2, 1},
    {29, "909 Snare 2", 2, 1},
    {30, "909 Low Tom", 3, 1},
    {31, "909 Mid Tom", 3, 1},
    {32, "909 High Tom", 3, 1},
    {36, "909 Rim Shot", 4, 1},
    {37, "909 Hand Clap", 5, 1},
    {38, "909 Closed HH", 6, 1},
    {39, "909 Open HH", 6, 1},
    {40, "909 Crash Cym", 7, 1},
    {41, "909 Ride Cym", 7, 1},
    {81, "808 Conga L Morph", 8, 1},
    {82, "808 Conga M Morph", 8, 1},
    {83, "808 Conga H Morph", 8, 1},
    {84, "909 Rim Morph", 4, 1},
    {85, "909 Clap Morph", 5, 1},
    {86, "FM Kick 1", 1, 3},
    {87, "FM Kick 2", 1, 3},
    {88, "FM Kick 3", 1, 3},
    {89, "FM Kick 4", 1, 3},
    {90, "FM Attack Kick", 1, 3},
    {91, "FM Sub Kick", 1, 3},
    {92, "FM Punch Kick", 1, 3},
    {93, "FM Deep Kick", 1, 3},
    {94, "FM Elektro Kick", 1, 3},
    {95, "FM Gabber Kick", 1, 3},
    {96, "FM Wood Kick", 1, 3},
    {97, "FM Snare 1", 2, 3},
    {98, "FM Snare 2", 2, 3},
    {99, "FM Snare 3", 2, 3},
    {100, "FM Tight Snare", 2, 3},
    {101, "FM Hollow Snare", 2, 3},
    {102, "FM Metal Snare", 2, 3},
    {103, "FM Zap Snare", 2, 3},
    {104, "FM Snap Snare", 2, 3},
    {105, "FM Crisp Snare", 2, 3},
    {106, "FM Elektro Snare", 2, 3},
    {107, "FM Noise Snare", 2, 3},
    {108, "FM Tom 1", 3, 3},
    {109, "FM Tom 2", 3, 3},
    {110, "FM Groover Tom", 3, 3},
    {111, "FM Sphere Tom", 3, 3},
    {112, "FM Space Tom", 3, 3},
    {113, "FM Rim Shot", 4, 3},
    {114, "FM Hand Clap 1", 5, 3},
    {115, "FM Hand Clap 2", 5, 3},
    {116, "FM Glitch Clap", 5, 3},
    {117, "FM Slap Clap", 5, 3},
    {118, "FM Closed HH 1", 6, 3},
    {119, "FM Closed HH 2", 6, 3},
    {120, "FM Open HH 1", 6, 3},
    {121, "FM Open HH 2", 6, 3},
    {122, "FM Crash Cym 1", 7, 3},
    {123, "FM Crash Cym 2", 7, 3},
    {124, "FM Stick Perc", 4, 3},
    {125, "FM Click Perc", 9, 3},
    {126, "FM Metal Rim", 4, 3},
    {127, "FM Ride Cym 1", 7, 3},
    {128, "FM Ride Cym 2", 7, 3},
    {129, "FM Bell Cym", 7, 3},
    {130, "FM Gong Cym", 7, 3},
    {131, "FM Bongo", 8, 3},
    {132, "FM Agogo", 11, 3},
    {133, "FM Conga", 8, 3},
    {134, "FM Timbale", 8, 3},
    {135, "FM Cowbell 1", 11, 3},
    {136, "FM Cowbell 2", 11, 3},
    {137, "FM Claves", 9, 3},
    {138, "FM Metal Perc", 11, 3},
    {139, "FM Triangle", 11, 3},
    {140, "FM Woodblock", 9, 3},
    {141, "FM Marimba", 18, 3},
    {142, "FM Vibraphone", 18, 3},
    {143, "FM Bass 1", 17, 3},
    {144, "FM Bass 2", 17, 3},
    {145, "FM Bell Synth", 15, 3},
    {146, "FM Pluck Synth", 15, 3},
    {147, "FM Chord Synth", 19, 3},
    {148, "FM Tube Perc", 13, 3},
    {149, "FM Riser FX", 13, 3},
    {150, "FM Laser FX", 13, 3},
    {151, "FM Drop FX", 13, 3},
    {152, "FMBubble FX", 13, 3},
    {153, "FM Sci-Fi FX", 13, 3},
    {154, "FM Warp FX", 13, 3},
    {155, "FM Glitch FX", 13, 3},
    {156, "FM Bleep Synth", 15, 3},
    {157, "909 Chromatic Bs", 17, 1},
    {158, "808 Chromatic Bs", 17, 1},
    {159, "CR-78 Bass", 1, 1},
    {160, "CR-78 Snare", 2, 1},
    {161, "CR-78 Rim Shot", 4, 1},
    {162, "CR-78 Closed HH", 6, 1},
    {163, "CR-78 Open HH", 6, 1},
    {164, "CR-78 Cymbal", 7, 1},
    {165, "CR-78 Low Bongo", 8, 1},
    {166, "CR-78 High Bongo", 8, 1},
    {167, "CR-78 Low Conga", 8, 1},
    {168, "CR-78 Maracas", 10, 1},
    {169, "CR-78 Cowbell", 11, 1},
    {170, "CR-78 Tambourine", 10, 1},
    {171, "CR-78 Guiro", 12, 1},
    {172, "CR-78 Metal Beat", 11, 1},
    {173, "CR-78 Claves", 9, 1},
};

struct KitDef {
    const char* name;
    uint16_t tones[kNumChannels];
};

const KitDef kKitDefs[] = {
    {"TR-808 Classic", {0, 4, 6, 7, 8, 15, 17, 21, 22, 23, 25}},
    {"TR-909 Studio", {26, 28, 30, 31, 32, 36, 37, 38, 39, 40, 41}},
    {"TR-707 Digital", {42, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55}},
    {"TR-727 Latin", {57, 59, 61, 62, 63, 64, 66, 67, 68, 69, 70}},
    {"TR-606 Acid", {72, 73, 74, 75, 76, 77, 78, 79, 80, 72, 73}},
    {"CR-78 Vintage", {159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169}},
    {"FM Hard Techno", {95, 102, 108, 109, 112, 113, 116, 118, 120, 122, 128}},
    {"Hybrid Warehouse", {0, 28, 31, 32, 33, 37, 40, 38, 39, 41, 25}},
};

constexpr uint8_t kRevSend[kNumChannels] = {12, 55, 32, 32, 32, 30, 45, 18, 34, 60, 60};
constexpr uint8_t kDlySend[kNumChannels] = {6, 26, 18, 18, 18, 25, 30, 10, 28, 55, 55};

void setCell(FirmwarePattern& p, int variation, int track, int step, uint8_t velocity, uint8_t flags = 0) {
    if (variation < 0 || variation >= kNumVariations) return;
    if (track < 0 || track >= kNumTracks) return;
    if (step < 0 || step >= kNumSteps) return;
    p.variations[variation].velocity[track][step] = velocity;
    p.variations[variation].flags[track][step] = flags;
}

void variationize(FirmwarePattern& p) {
    for (int v = 1; v < kNumVariations; ++v)
        p.variations[v] = p.variations[0];

    // B: 16th hats and ghost snares.
    for (int s = 0; s < kNumSteps; ++s)
        if (p.variations[1].velocity[7][s] == 0) setCell(p, 1, 7, s, 58);
    setCell(p, 1, 1, 7, 48);
    setCell(p, 1, 1, 15, 52);

    // C: half-time, longer tail.
    setCell(p, 2, 0, 8, 0);
    setCell(p, 2, 1, 12, 0);
    setCell(p, 2, 1, 8, 96);
    setCell(p, 2, 8, 14, 88);
    setCell(p, 2, 9, 15, 70);

    // D: tom fill at the end of the bar.
    setCell(p, 3, 2, 12, 94);
    setCell(p, 3, 3, 13, 98);
    setCell(p, 3, 4, 14, 102);
    setCell(p, 3, 1, 15, 110);
    setCell(p, 3, 11, 15, 105);

    // E: open-hat lift.
    setCell(p, 4, 8, 2, 78);
    setCell(p, 4, 8, 6, 82);
    setCell(p, 4, 8, 10, 82);
    setCell(p, 4, 8, 14, 88);

    // F: clap/rim conversation.
    setCell(p, 5, 6, 4, 92);
    setCell(p, 5, 6, 12, 96);
    setCell(p, 5, 5, 7, 74);
    setCell(p, 5, 5, 15, 80);

    // G: stripped breakdown.
    for (int s = 0; s < kNumSteps; ++s) {
        p.variations[6].velocity[7][s] = (s % 4 == 0) ? 60 : 0;
        p.variations[6].velocity[8][s] = 0;
        p.variations[6].velocity[9][s] = 0;
    }
    setCell(p, 6, 0, 0, 108);
    setCell(p, 6, 0, 8, 108);
    setCell(p, 6, 1, 12, 100);

    // H: fill-in variation used by the FILL button.
    for (int s = 0; s < kNumSteps; ++s) {
        p.variations[7].velocity[7][s] = 70;
        if (s >= 12) {
            p.variations[7].velocity[2][s] = 88 + static_cast<uint8_t>(s - 12) * 6;
            p.variations[7].velocity[1][s] = 76;
        }
    }
    setCell(p, 7, 11, 15, 115);
}

void clearPattern(FirmwarePattern& p) {
    for (auto& v : p.variations) {
        std::memset(v.velocity, 0, sizeof(v.velocity));
        std::memset(v.flags, 0, sizeof(v.flags));
    }
}

void setCode(FirmwarePattern& p, int index) {
    const int bank = index / 16 + 1;
    const int slot = index % 16 + 1;
    std::snprintf(p.code, sizeof(p.code), "%d-%02d", bank, slot);
}

} // namespace

void buildPreviewBank(FirmwareBank& bank) {
    bank = FirmwareBank {};

    for (int i = 0; i < kNumTones; ++i) {
        bank.tones[i].id = static_cast<uint16_t>(i);
        bank.tones[i].category = static_cast<uint8_t>((i % 11) + 1);
        bank.tones[i].engineType = 1;
        std::snprintf(bank.tones[i].name, sizeof(bank.tones[i].name), "Tone %03d", i);
    }
    for (const auto& meta : kToneMeta) {
        if (meta.id < 0 || meta.id >= kNumTones) continue;
        auto& t = bank.tones[meta.id];
        std::snprintf(t.name, sizeof(t.name), "%s", meta.name);
        t.category = static_cast<uint8_t>(meta.category);
        t.engineType = static_cast<uint8_t>(meta.engine);
    }

    for (int k = 0; k < kNumKits; ++k) {
        auto& kit = bank.kits[k];
        kit.number = static_cast<uint8_t>(k + 1);
        std::snprintf(kit.name, sizeof(kit.name), "INIT KIT %03d", k + 1);
        for (int ch = 0; ch < kNumChannels; ++ch) {
            auto& cp = kit.channels[ch];
            cp.toneId = static_cast<uint16_t>(ch == 0 ? 0 : (ch == 1 ? 4 : 21));
            cp.tune = 128;
            cp.decay = 128;
            cp.level = 150;
            cp.gain = 81;
            cp.pan = 128;
            cp.revSend = kRevSend[ch];
            cp.dlySend = kDlySend[ch];
            cp.ctrl = 128;
            cp.fxType = 0;
        }
    }

    for (int i = 0; i < kNumPatterns; ++i) {
        auto& p = bank.patterns[i];
        p.number = static_cast<uint8_t>(i + 1);
        setCode(p, i);
        std::snprintf(p.name, sizeof(p.name), "INIT PTN %d-%02d", i / 16 + 1, i % 16 + 1);
        p.tempoTenths = 1280;
        p.kitNumber = 1;
        clearPattern(p);
    }

    int kitIndex = 0;
    for (const auto& def : kKitDefs) {
        auto& kit = bank.kits[kitIndex];
        std::snprintf(kit.name, sizeof(kit.name), "%s", def.name);
        for (int ch = 0; ch < kNumChannels; ++ch) {
            kit.channels[ch].toneId = def.tones[ch];
            kit.channels[ch].revSend = kRevSend[ch];
            kit.channels[ch].dlySend = kDlySend[ch];
        }
        ++kitIndex;
    }

    auto build = [&](int index, const char* name, int kitIndex, int bpm, const char* code) {
        auto& p = bank.patterns[index];
        clearPattern(p);
        std::snprintf(p.name, sizeof(p.name), "%s", name);
        p.kitNumber = static_cast<uint8_t>(kitIndex + 1);
        p.tempoTenths = static_cast<uint16_t>(bpm * 10);
        if (code != nullptr) std::snprintf(p.code, sizeof(p.code), "%s", code);
        return &p;
    };

    // 1-01 Warehouse 909
    if (auto* p = build(0, "Warehouse 909", 1, 132, "1-01")) {
        for (int s = 0; s < 16; s += 4) { setCell(*p, 0, 0, s, 112); setCell(*p, 0, 11, s, 100); }
        setCell(*p, 0, 1, 4, 106); setCell(*p, 0, 1, 12, 106);
        for (int s = 0; s < 16; s += 2) setCell(*p, 0, 7, s, 80);
        for (int s = 2; s < 16; s += 4) setCell(*p, 0, 8, s, 92);
        setCell(*p, 0, 5, 7, 70); setCell(*p, 0, 5, 15, 74);
        setCell(*p, 0, 9, 5, 82); setCell(*p, 0, 9, 13, 86);
        setCell(*p, 0, 10, 14, 72);
        variationize(*p);
    }

    // 1-02 Berlin Dub 808
    if (auto* p = build(1, "Berlin Dub 808", 0, 126, "1-02")) {
        setCell(*p, 0, 0, 0, 115); setCell(*p, 0, 0, 3, 98); setCell(*p, 0, 0, 8, 112); setCell(*p, 0, 0, 11, 96);
        setCell(*p, 0, 1, 4, 100); setCell(*p, 0, 1, 12, 100);
        for (int s = 0; s < 16; s += 2) setCell(*p, 0, 7, s, 72);
        setCell(*p, 0, 8, 2, 86); setCell(*p, 0, 8, 10, 86);
        setCell(*p, 0, 6, 4, 78); setCell(*p, 0, 6, 12, 82);
        setCell(*p, 0, 5, 7, 66); setCell(*p, 0, 5, 15, 70);
        setCell(*p, 0, 9, 13, 84);
        variationize(*p);
    }

    // 1-03 Acid 606
    if (auto* p = build(2, "Acid 606", 4, 136, "1-03")) {
        for (int s = 0; s < 16; s += 4) setCell(*p, 0, 0, s, 108);
        setCell(*p, 0, 1, 12, 104);
        for (int s = 0; s < 16; ++s) setCell(*p, 0, 7, s, 68);
        setCell(*p, 0, 8, 2, 82); setCell(*p, 0, 8, 6, 82);
        setCell(*p, 0, 8, 10, 82); setCell(*p, 0, 8, 14, 86);
        setCell(*p, 0, 9, 5, 90); setCell(*p, 0, 9, 13, 88);
        setCell(*p, 0, 10, 15, 76);
        variationize(*p);
    }

    // 1-04 Detroit 909
    if (auto* p = build(3, "Detroit 909", 1, 130, "1-04")) {
        for (int s = 0; s < 16; s += 4) setCell(*p, 0, 0, s, 110);
        setCell(*p, 0, 1, 4, 102); setCell(*p, 0, 1, 12, 102);
        for (int s = 2; s < 16; s += 4) setCell(*p, 0, 7, s, 84);
        setCell(*p, 0, 8, 6, 78); setCell(*p, 0, 8, 14, 82);
        setCell(*p, 0, 5, 7, 72); setCell(*p, 0, 5, 15, 76);
        setCell(*p, 0, 10, 10, 74);
        setCell(*p, 0, 9, 13, 78);
        variationize(*p);
    }

    // 1-05 Latin 727
    if (auto* p = build(4, "Latin 727", 3, 120, "1-05")) {
        setCell(*p, 0, 0, 0, 104); setCell(*p, 0, 0, 6, 94); setCell(*p, 0, 0, 8, 102); setCell(*p, 0, 0, 14, 92);
        setCell(*p, 0, 1, 4, 96); setCell(*p, 0, 1, 12, 96);
        setCell(*p, 0, 5, 3, 74); setCell(*p, 0, 5, 11, 78);
        setCell(*p, 0, 6, 4, 86); setCell(*p, 0, 6, 12, 90);
        for (int s = 2; s < 16; s += 4) setCell(*p, 0, 7, s, 70);
        setCell(*p, 0, 8, 6, 78); setCell(*p, 0, 8, 14, 80);
        setCell(*p, 0, 9, 5, 84); setCell(*p, 0, 9, 13, 86);
        setCell(*p, 0, 10, 15, 80);
        setCell(*p, 0, 2, 7, 66); setCell(*p, 0, 3, 15, 70);
        variationize(*p);
    }

    // 1-06 Electro 808
    if (auto* p = build(5, "Electro 808", 0, 128, "1-06")) {
        for (int s = 0; s < 16; s += 4) setCell(*p, 0, 0, s, 114);
        setCell(*p, 0, 1, 4, 104); setCell(*p, 0, 1, 12, 104);
        for (int s = 0; s < 16; ++s) setCell(*p, 0, 7, s, (s % 2 == 0) ? 78 : 50);
        setCell(*p, 0, 8, 6, 86); setCell(*p, 0, 8, 14, 86);
        setCell(*p, 0, 6, 4, 80); setCell(*p, 0, 6, 12, 84);
        setCell(*p, 0, 9, 3, 86); setCell(*p, 0, 9, 11, 88);
        setCell(*p, 0, 10, 15, 78);
        variationize(*p);
    }

    // 1-07 Hard Techno FM
    if (auto* p = build(6, "Hard Techno FM", 6, 148, "1-07")) {
        for (int s = 0; s < 16; s += 4) { setCell(*p, 0, 0, s, 118); setCell(*p, 0, 11, s, 112); }
        setCell(*p, 0, 1, 4, 112); setCell(*p, 0, 1, 12, 112);
        for (int s = 0; s < 16; ++s) setCell(*p, 0, 7, s, (s % 4 == 0) ? 86 : 70);
        for (int s = 2; s < 16; s += 4) setCell(*p, 0, 8, s, 100);
        setCell(*p, 0, 9, 3, 90); setCell(*p, 0, 9, 7, 92); setCell(*p, 0, 9, 11, 92); setCell(*p, 0, 9, 15, 96);
        setCell(*p, 0, 10, 15, 88);
        variationize(*p);
    }

    // 1-08 CR-78 Boogie
    if (auto* p = build(7, "CR-78 Boogie", 5, 112, "1-08")) {
        setCell(*p, 0, 0, 0, 104); setCell(*p, 0, 0, 8, 102);
        setCell(*p, 0, 1, 4, 94); setCell(*p, 0, 1, 12, 96);
        for (int s = 2; s < 16; s += 4) setCell(*p, 0, 7, s, 70);
        setCell(*p, 0, 8, 6, 74); setCell(*p, 0, 8, 14, 78);
        setCell(*p, 0, 6, 4, 76); setCell(*p, 0, 6, 12, 80);
        setCell(*p, 0, 9, 3, 80); setCell(*p, 0, 9, 11, 82);
        setCell(*p, 0, 10, 15, 74);
        variationize(*p);
    }

    // 1-09 Trap 808
    if (auto* p = build(8, "Trap 808", 0, 140, "1-09")) {
        setCell(*p, 0, 0, 0, 116); setCell(*p, 0, 0, 7, 104); setCell(*p, 0, 0, 10, 106); setCell(*p, 0, 0, 14, 100);
        setCell(*p, 0, 1, 4, 108); setCell(*p, 0, 1, 12, 108);
        for (int s = 0; s < 16; ++s) setCell(*p, 0, 7, s, (s % 2 == 0) ? 66 : 44);
        setCell(*p, 0, 8, 2, 72); setCell(*p, 0, 8, 6, 74); setCell(*p, 0, 8, 10, 74);
        setCell(*p, 0, 9, 13, 84);
        setCell(*p, 0, 10, 15, 76);
        variationize(*p);
    }

    // 1-10 Industrial 909
    if (auto* p = build(9, "Industrial 909", 1, 138, "1-10")) {
        for (int s = 0; s < 16; s += 4) setCell(*p, 0, 0, s, 112);
        setCell(*p, 0, 1, 2, 104); setCell(*p, 0, 1, 6, 104); setCell(*p, 0, 1, 10, 106); setCell(*p, 0, 1, 14, 106);
        for (int s = 0; s < 16; ++s) setCell(*p, 0, 7, s, 74);
        for (int s = 1; s < 16; s += 4) setCell(*p, 0, 8, s, 88);
        setCell(*p, 0, 5, 3, 80); setCell(*p, 0, 5, 11, 84);
        setCell(*p, 0, 9, 3, 90); setCell(*p, 0, 9, 11, 90);
        setCell(*p, 0, 10, 15, 82);
        variationize(*p);
    }

    // 1-11 Chicago 707
    if (auto* p = build(10, "Chicago 707", 2, 124, "1-11")) {
        for (int s = 0; s < 16; s += 4) setCell(*p, 0, 0, s, 106);
        setCell(*p, 0, 1, 4, 100); setCell(*p, 0, 1, 12, 100);
        for (int s = 0; s < 16; s += 2) setCell(*p, 0, 7, s, 76);
        for (int s = 2; s < 16; s += 4) setCell(*p, 0, 8, s, 82);
        setCell(*p, 0, 9, 5, 84); setCell(*p, 0, 9, 13, 86);
        setCell(*p, 0, 10, 11, 72);
        variationize(*p);
    }

    // 1-12 Rave 909
    if (auto* p = build(11, "Rave 909", 1, 146, "1-12")) {
        for (int s = 0; s < 16; s += 4) { setCell(*p, 0, 0, s, 116); setCell(*p, 0, 11, s, 108); }
        setCell(*p, 0, 1, 4, 108); setCell(*p, 0, 1, 12, 108);
        for (int s = 0; s < 16; ++s) setCell(*p, 0, 7, s, 78);
        for (int s = 2; s < 16; s += 4) setCell(*p, 0, 8, s, 94);
        setCell(*p, 0, 9, 3, 88); setCell(*p, 0, 9, 7, 90); setCell(*p, 0, 9, 11, 90); setCell(*p, 0, 9, 15, 96);
        setCell(*p, 0, 5, 7, 74); setCell(*p, 0, 5, 15, 78);
        setCell(*p, 0, 10, 15, 84);
        variationize(*p);
    }

    // 7-08 Straight 909 (factory default)
    if (auto* p = build(103, "Straight 909", 1, 128, "7-08")) {
        for (int s = 0; s < 16; s += 4) { setCell(*p, 0, 0, s, 112); setCell(*p, 0, 11, s, 100); }
        setCell(*p, 0, 1, 4, 104); setCell(*p, 0, 1, 12, 104);
        for (int s = 0; s < 16; s += 2) setCell(*p, 0, 7, s, 80);
        for (int s = 2; s < 16; s += 4) setCell(*p, 0, 8, s, 88);
        setCell(*p, 0, 6, 4, 84); setCell(*p, 0, 6, 12, 86);
        setCell(*p, 0, 9, 13, 80);
        variationize(*p);
    }

    bank.loaded = true;
    std::snprintf(bank.name, sizeof(bank.name), "Built-in preview");
}

namespace {

uint32_t readLE32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

uint16_t readLE16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

void copyTrimmedAscii(char* dst, size_t cap, const uint8_t* src, size_t len) {
    if (cap == 0) return;
    const size_t n = std::min(cap - 1, len);
    std::memcpy(dst, src, n);
    dst[n] = '\0';
    for (size_t i = 0; i < n; ++i) {
        const unsigned char c = static_cast<unsigned char>(dst[i]);
        if (c < 0x20 || c > 0x7e) dst[i] = ' ';
    }
    int end = static_cast<int>(n) - 1;
    while (end >= 0 && (dst[end] == ' ' || dst[end] == '\0')) {
        dst[end] = '\0';
        --end;
    }
}

std::vector<uint8_t> decompressLZSS(const uint8_t* src, size_t srcLen, size_t uncompLen) {
    constexpr size_t N = 4096;
    constexpr size_t F = 18;
    constexpr size_t THRESHOLD = 2;

    std::array<uint8_t, N> ring {};
    size_t r = N - F;
    std::vector<uint8_t> out;
    out.reserve(uncompLen);

    size_t i = 0;
    uint32_t flags = 0;
    while (i < srcLen && out.size() < uncompLen) {
        flags >>= 1;
        if ((flags & 0x100u) == 0) {
            if (i >= srcLen) break;
            flags = static_cast<uint32_t>(src[i++]) | 0xFF00u;
        }
        if (flags & 1u) {
            if (i >= srcLen) break;
            const uint8_t c = src[i++];
            out.push_back(c);
            ring[r] = c;
            r = (r + 1) & (N - 1);
        } else {
            if (i + 1 >= srcLen) break;
            const uint8_t b0 = src[i++];
            const uint8_t b1 = src[i++];
            const size_t offset = static_cast<size_t>(b0) | (static_cast<size_t>(b1 & 0xF0u) << 4);
            const size_t length = static_cast<size_t>(b1 & 0x0Fu) + THRESHOLD + 1;
            for (size_t k = 0; k < length && out.size() < uncompLen; ++k) {
                const uint8_t c = ring[(offset + k) & (N - 1)];
                out.push_back(c);
                ring[r] = c;
                r = (r + 1) & (N - 1);
            }
        }
    }
    return out;
}

bool parseInitParamBin(const uint8_t* data, size_t size, FirmwareBank& bank, std::string& error) {
    if (size < 0x40 || std::memcmp(data, "INIT", 4) != 0) {
        error = "Invalid rpg42_init_param.bin header (missing INIT signature).";
        return false;
    }

    const uint32_t compSize = readLE32(data + 8);
    const uint32_t uncompSize = readLE32(data + 20);
    constexpr uint32_t kMaxUncompressed = 16u * 1024u * 1024u;
    if (compSize == 0 || 0x30u + compSize > size || uncompSize < 0x330000u ||
        uncompSize > kMaxUncompressed) {
        error = "Corrupted or oversized LZSS size fields in firmware parameter binary.";
        return false;
    }

    auto unpacked = decompressLZSS(data + 0x30, compSize, uncompSize);
    if (unpacked.size() < 0x330000 || std::memcmp(unpacked.data(), "TR8S", 4) != 0) {
        error = "LZSS decompression failed or TR8S magic missing.";
        return false;
    }

    constexpr size_t tonePos = 0x326F90 + 32;
    for (int i = 0; i < kNumTones; ++i) {
        const uint8_t* t = unpacked.data() + tonePos + static_cast<size_t>(i) * 36;
        auto& tone = bank.tones[i];
        tone.id = static_cast<uint16_t>(i);
        copyTrimmedAscii(tone.name, sizeof(tone.name), t, 16);
        tone.category = t[16];
        tone.engineType = t[17];

        if (tone.name[0] == '\0') {
            bool found = false;
            for (const auto& meta : kToneMeta) {
                if (meta.id == i) {
                    std::snprintf(tone.name, sizeof(tone.name), "%s", meta.name);
                    tone.category = static_cast<uint8_t>(meta.category);
                    tone.engineType = static_cast<uint8_t>(meta.engine);
                    found = true;
                    break;
                }
            }
            if (!found) std::snprintf(tone.name, sizeof(tone.name), "Tone %03d", i);
        }
    }

    constexpr size_t kitPos = 0x2FDF70 + 32;
    for (int kidx = 0; kidx < kNumKits; ++kidx) {
        const uint8_t* k = unpacked.data() + kitPos + static_cast<size_t>(kidx) * 0x520;
        auto& kit = bank.kits[kidx];
        kit.number = static_cast<uint8_t>(kidx + 1);
        copyTrimmedAscii(kit.name, sizeof(kit.name), k, 16);
        if (kit.name[0] == '\0') std::snprintf(kit.name, sizeof(kit.name), "KIT %03d", kidx + 1);

        for (int ch = 0; ch < kNumChannels; ++ch) {
            const uint8_t* slot = k + 0x184 + static_cast<size_t>(ch) * 0x34;
            const uint8_t* fx = k + 0x3C0 + static_cast<size_t>(ch) * 0x20;
            auto& cp = kit.channels[ch];
            const uint16_t tid = readLE16(slot);
            cp.toneId = (tid < kNumTones) ? tid : 0;
            cp.tune = slot[2];
            cp.decay = slot[3];
            cp.level = 150;
            cp.gain = slot[5];
            cp.pan = slot[6];
            cp.revSend = slot[7];
            cp.dlySend = slot[8];
            cp.ctrl = slot[28];
            cp.fxType = fx[0];
        }
    }

    constexpr size_t ptnPos = 0x000350 + 32;
    for (int pidx = 0; pidx < kNumPatterns; ++pidx) {
        const uint8_t* p = unpacked.data() + ptnPos + static_cast<size_t>(pidx) * 0x5FB8;
        auto& ptn = bank.patterns[pidx];
        ptn.number = static_cast<uint8_t>(pidx + 1);
        setCode(ptn, pidx);
        copyTrimmedAscii(ptn.name, sizeof(ptn.name), p, 16);
        if (ptn.name[0] == '\0')
            std::snprintf(ptn.name, sizeof(ptn.name), "PTN %d-%02d", pidx / 16 + 1, pidx % 16 + 1);

        const uint16_t tempoRaw = readLE16(p + 16);
        ptn.tempoTenths = (tempoRaw >= 400 && tempoRaw <= 3000) ? tempoRaw : 1280;
        const uint8_t kitNo = p[18];
        ptn.kitNumber = (kitNo >= 1 && kitNo <= kNumKits) ? kitNo : 1;

        for (int v = 0; v < kNumVariations; ++v) {
            const uint8_t* var = p + 0x90 + static_cast<size_t>(v) * 0x984;
            for (int trk = 0; trk < kNumTracks; ++trk) {
                for (int s = 0; s < kNumSteps; ++s) {
                    const size_t stepOff = 4 + static_cast<size_t>(trk) * 64 + static_cast<size_t>(s) * 4;
                    ptn.variations[v].velocity[trk][s] = var[stepOff];
                    ptn.variations[v].flags[trk][s] = var[stepOff + 1];
                }
            }
        }
    }

    bank.loaded = true;
    bank.imported = true;
    std::snprintf(bank.name, sizeof(bank.name), "TR-8S v3.00 firmware");
    return true;
}

bool parseTarArchive(const uint8_t* bytes, size_t size, FirmwareBank& bank, std::string& error) {
    size_t pos = 0;
    while (pos + 512 <= size) {
        const uint8_t* hdr = bytes + pos;
        if (hdr[0] == '\0') break;

        char nameBuf[101] {};
        std::memcpy(nameBuf, hdr, 100);
        const std::string entryName(nameBuf);

        char sizeOctal[13] {};
        std::memcpy(sizeOctal, hdr + 124, 12);
        const size_t fileSize = static_cast<size_t>(std::strtoull(sizeOctal, nullptr, 8));

        pos += 512;
        if (pos + fileSize > size) break;

        const bool named = entryName.find("init_param.bin") != std::string::npos;
        const bool magic = fileSize >= 0x40 && std::memcmp(bytes + pos, "INIT", 4) == 0;
        if (named || magic)
            return parseInitParamBin(bytes + pos, fileSize, bank, error);

        pos += ((fileSize + 511) / 512) * 512;
    }

    error = "Could not find rpg42_init_param.bin inside TAR archive (TR8S_UP.bin).";
    return false;
}

} // namespace

bool parseFirmwareBytes(const uint8_t* data, size_t size, FirmwareBank& bank, std::string& error) {
    error.clear();
    bank = FirmwareBank {};
    if (data == nullptr || size < 4) {
        error = "Firmware file is empty.";
        return false;
    }
    if (std::memcmp(data, "INIT", 4) == 0) {
        if (parseInitParamBin(data, size, bank, error)) return true;
        bank = FirmwareBank {};
        return false;
    }
    if (parseTarArchive(data, size, bank, error)) return true;
    bank = FirmwareBank {};
    return false;
}

} // namespace rt8
