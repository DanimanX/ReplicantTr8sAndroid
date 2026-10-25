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
    {2, "808 Bass 1 Long", 1, 1},
    {3, "808 Bass 2 Long", 1, 1},
    {4, "808 Snare 1", 2, 1},
    {5, "808 Snare 2", 2, 1},
    {6, "808 Low Tom", 3, 1},
    {7, "808 Mid Tom", 3, 1},
    {8, "808 High Tom", 3, 1},
    {9, "808 Low Conga", 8, 1},
    {10, "808 Mid Conga", 8, 1},
    {11, "808 High Conga", 8, 1},
    {12, "808 Noise Tom L", 3, 1},
    {13, "808 Noise Tom M", 3, 1},
    {14, "808 Noise Tom H", 3, 1},
    {15, "808 Rim Shot", 4, 1},
    {16, "808 Claves", 9, 1},
    {17, "808 Hand Clap", 5, 1},
    {18, "808 Maracas", 10, 1},
    {19, "808 Finger Snap", 5, 1},
    {20, "808 Noise Clap", 5, 1},
    {21, "808 Closed HH", 6, 1},
    {22, "808 Open HH", 6, 1},
    {23, "808 Cymbal", 7, 1},
    {24, "808 Gate Cymbal", 7, 1},
    {25, "808 Cowbell", 11, 1},
    {26, "909 Bass 1", 1, 1},
    {27, "909 Bass 2", 1, 1},
    {28, "909 Attack Bass", 1, 1},
    {29, "909 Low Bass", 1, 1},
    {30, "909 Snare 1", 2, 1},
    {31, "909 Snare 2", 2, 1},
    {32, "909 Attack Snare", 2, 1},
    {33, "909 Low Tom", 3, 1},
    {34, "909 Mid Tom", 3, 1},
    {35, "909 High Tom", 3, 1},
    {36, "909 Rim Shot", 4, 1},
    {37, "909 Hand Clap", 5, 1},
    {38, "909 Closed HH", 6, 1},
    {39, "909 Open HH", 6, 1},
    {40, "909 Crash Cymbal", 7, 1},
    {41, "909 Ride Cymbal", 7, 1},
    {42, "707 Bass 1", 1, 1},
    {43, "707 Bass 2", 1, 1},
    {44, "707 Snare 1", 2, 1},
    {45, "707 Snare 2", 2, 1},
    {46, "707 Low Tom", 3, 1},
    {47, "707 Mid Tom", 3, 1},
    {48, "707 High Tom", 3, 1},
    {49, "707 Rim Shot", 4, 1},
    {50, "707 Cowbell", 11, 1},
    {51, "707 Hand Clap", 5, 1},
    {52, "707 Tambourine", 10, 1},
    {53, "707 Closed HH", 6, 1},
    {54, "707 Open HH", 6, 1},
    {55, "707 Crash Cymbal", 7, 1},
    {56, "707 Ride Cymbal", 7, 1},
    {57, "727 High Bongo", 8, 1},
    {58, "727 Low Bongo", 8, 1},
    {59, "727 Mute HiConga", 8, 1},
    {60, "727 Open HiConga", 8, 1},
    {61, "727 Low Conga", 8, 1},
    {62, "727 High Timbale", 8, 1},
    {63, "727 Low Timbale", 8, 1},
    {64, "727 High Agogo", 11, 1},
    {65, "727 Low Agogo", 11, 1},
    {66, "727 Cabasa", 10, 1},
    {67, "727 Maracas", 10, 1},
    {68, "727 ShortWhistle", 12, 1},
    {69, "727 Long Whistle", 12, 1},
    {70, "727 Quijada", 12, 1},
    {71, "727 Star Chime", 12, 1},
    {72, "606 Bass", 1, 1},
    {73, "606 Snare", 2, 1},
    {74, "606 Low Tom", 3, 1},
    {75, "606 Mid Tom", 3, 1},
    {76, "606 High Tom", 3, 1},
    {77, "606 Closed HH", 6, 1},
    {78, "606 Open HH", 6, 1},
    {79, "606 Cymbal", 7, 1},
    {80, "606 Gate Cymbal", 7, 1},
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

void setCode(FirmwarePattern& p, int index) {
    const int bank = index / 16 + 1;
    const int slot = index % 16 + 1;
    std::snprintf(p.code, sizeof(p.code), "%d-%02d", bank, slot);
}

} // namespace

uint8_t inferToneCategory(int id) {
    if (id >= 181 && id <= 205) return 1;  // PCM kicks
    if (id >= 206 && id <= 248) return 2;  // PCM snares
    if (id >= 249 && id <= 262) return 3;  // PCM toms
    if (id >= 263 && id <= 270) return 4;  // PCM rim shots
    if (id >= 271 && id <= 290) return 5;  // PCM claps / snaps
    if (id >= 291 && id <= 315) return 6;  // PCM hi-hats
    if (id >= 316 && id <= 322) return 7;  // PCM cymbals
    if (id >= 323 && id <= 334) return 8;  // PCM congas / timbales
    if (id >= 335 && id <= 340) return 9;  // PCM claves / blocks
    if (id >= 341 && id <= 347) return 10; // PCM shakers / tambourines
    if (id >= 348 && id <= 354) return 11; // PCM cowbells / agogos / triangles
    if (id >= 355 && id <= 358) return 12; // Jingle / udu
    if (id >= 359 && id <= 415) return 13; // FX / hits / zaps
    if (id >= 416 && id <= 434) return 14; // Voice / formants
    if (id >= 435 && id <= 450) return 15; // Synth leads / pads
    if (id >= 451 && id <= 475) return 16; // OSC waveforms
    if (id >= 476 && id <= 490) return 17; // Basses
    if (id >= 491 && id <= 504) return 18; // Pianos / mallets / brass
    if (id >= 505 && id <= 521) return 19; // Chords
    return 1;
}

bool toneMetaForId(int toneId, ToneMetaInfo& out) {
    for (const auto& meta : kToneMeta) {
        if (meta.id == toneId) {
            out.name = meta.name;
            out.category = static_cast<uint8_t>(meta.category);
            out.engineType = static_cast<uint8_t>(meta.engine);
            return true;
        }
    }
    return false;
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
            ToneMetaInfo meta;
            if (toneMetaForId(i, meta)) {
                std::snprintf(tone.name, sizeof(tone.name), "%s", meta.name);
                tone.category = meta.category;
                tone.engineType = meta.engineType;
            } else {
                std::snprintf(tone.name, sizeof(tone.name), "Tone %03d", i);
                if (tone.category == 0) tone.category = inferToneCategory(i);
            }
        }
    }

    constexpr size_t kitPos = 0x2FDF70 + 32;
    for (int kidx = 0; kidx < kNumKits; ++kidx) {
        const uint8_t* k = unpacked.data() + kitPos + static_cast<size_t>(kidx) * 0x520;
        auto& kit = bank.kits[kidx];
        kit.number = static_cast<uint8_t>(kidx + 1);
        copyTrimmedAscii(kit.name, sizeof(kit.name), k, 16);
        if (kit.name[0] == '\0') std::snprintf(kit.name, sizeof(kit.name), "KIT %03d", kidx + 1);

        // Kit master FX levels scale the per-channel sends. The classic vintage
        // kits 1..6 ship them at 0, so those channels must stay completely dry.
        const uint8_t kitRevMaster = k[0x46];
        const uint8_t kitDlyMaster = k[0x6E];

        for (int ch = 0; ch < kNumChannels; ++ch) {
            const uint8_t* slot = k + 0x184 + static_cast<size_t>(ch) * 0x34;
            const uint8_t* fx = k + 0x3C0 + static_cast<size_t>(ch) * 0x20;
            auto& cp = kit.channels[ch];
            const uint16_t tid = readLE16(slot);
            cp.toneId = (tid < kNumTones) ? tid : 0;
            cp.tune = slot[2];
            // PCM tones store 255 for nominal full-sample decay; centre at 128.
            cp.decay = (cp.toneId >= 181 && slot[3] >= 250) ? 128 : slot[3];
            cp.level = 150;
            cp.gain = slot[5];
            cp.pan = slot[6];
            cp.revSend = static_cast<uint8_t>((static_cast<uint32_t>(slot[7]) * kitRevMaster) / 255u);
            cp.dlySend = static_cast<uint8_t>((static_cast<uint32_t>(slot[8]) * kitDlyMaster) / 255u);
            // Non-morph ACB and PCM tones store filter flags [24, 200] at
            // slot[28..29]; centre CTRL instead of pinning it to the low end.
            cp.ctrl = (slot[28] == 24 && slot[29] == 200) ? 128 : slot[28];
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
