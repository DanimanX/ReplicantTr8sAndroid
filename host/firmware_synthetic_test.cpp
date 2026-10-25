// Builds a synthetic rpg42_init_param.bin in memory and pushes it through the
// real firmware parser. Covers the parts of the TR-8S image format the Android
// port has to interpret itself: tone metadata fallback, PCM decay/CTRL
// normalisation and the kit master reverb/delay scaling.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "engine.h"
#include "firmware.h"

namespace {

constexpr size_t kUncompressedSize = 0x330000;
constexpr size_t kToneChunk = 0x326F90 + 32;
constexpr size_t kKitChunk = 0x2FDF70 + 32;
constexpr size_t kKitStride = 0x520;
constexpr size_t kSlotBase = 0x184;
constexpr size_t kSlotStride = 0x34;

int failures = 0;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

bool near(float a, float b, float tol = 1.0e-6f) {
    return std::fabs(a - b) <= tol;
}

void writeLE16(uint8_t* p, uint16_t v) {
    p[0] = static_cast<uint8_t>(v & 0xFF);
    p[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
}

void writeLE32(uint8_t* p, uint32_t v) {
    for (int i = 0; i < 4; ++i) p[i] = static_cast<uint8_t>((v >> (8 * i)) & 0xFF);
}

// The parser's LZSS decoder accepts a control byte of 0xFF followed by eight
// literals, which is enough to emit an uncompressed image.
std::vector<uint8_t> compressLiteralOnly(const std::vector<uint8_t>& src) {
    std::vector<uint8_t> out;
    out.reserve(src.size() + src.size() / 8 + 8);
    size_t i = 0;
    while (i < src.size()) {
        out.push_back(0xFF);
        for (int b = 0; b < 8; ++b) {
            out.push_back(i < src.size() ? src[i++] : 0);
        }
    }
    return out;
}

std::vector<uint8_t> makeImage() {
    std::vector<uint8_t> raw(kUncompressedSize, 0);
    std::memcpy(raw.data(), "TR8S", 4);

    uint8_t* tone0 = raw.data() + kToneChunk;
    std::memcpy(tone0, "PCM SNARE", 9);
    tone0[16] = 6; // hi-hat category, deliberately not what the id range implies
    tone0[17] = 2; // PCM engine

    auto slot = [&](size_t kit, size_t ch) {
        return raw.data() + kKitChunk + kit * kKitStride + kSlotBase + ch * kSlotStride;
    };
    auto kitBytes = [&](size_t kit) { return raw.data() + kKitChunk + kit * kKitStride; };

    // Kit 001: full reverb master, muted delay master.
    std::memcpy(kitBytes(0), "SYNTH KIT", 9);
    kitBytes(0)[0x46] = 255;
    kitBytes(0)[0x6E] = 0;
    writeLE16(slot(0, 0), 200);    // PCM tone -> decay 255 normalises to 128
    slot(0, 0)[2] = 128;           // tune
    slot(0, 0)[3] = 255;           // decay
    slot(0, 0)[5] = 81;            // gain
    slot(0, 0)[6] = 128;           // pan
    slot(0, 0)[7] = 200;           // reverb send
    slot(0, 0)[8] = 180;           // delay send
    slot(0, 0)[28] = 24;           // filter flag pair -> CTRL normalises to 128
    slot(0, 0)[29] = 200;
    writeLE16(slot(0, 1), 21);
    slot(0, 1)[2] = 128;
    slot(0, 1)[3] = 200;
    slot(0, 1)[5] = 81;
    slot(0, 1)[6] = 128;
    slot(0, 1)[7] = 200;
    slot(0, 1)[8] = 180;
    slot(0, 1)[28] = 90;           // real CTRL value, must survive

    // Kit 002: muted reverb master, full delay master.
    std::memcpy(kitBytes(1), "AUX KIT", 7);
    kitBytes(1)[0x46] = 0;
    kitBytes(1)[0x6E] = 255;
    writeLE16(slot(1, 0), 200);
    slot(1, 0)[2] = 128;
    slot(1, 0)[3] = 200;
    slot(1, 0)[5] = 81;
    slot(1, 0)[6] = 128;
    slot(1, 0)[7] = 200;
    slot(1, 0)[8] = 180;
    slot(1, 0)[28] = 128;

    const std::vector<uint8_t> compressed = compressLiteralOnly(raw);
    std::vector<uint8_t> image(0x30 + compressed.size(), 0);
    std::memcpy(image.data(), "INIT", 4);
    writeLE32(image.data() + 8, static_cast<uint32_t>(compressed.size()));
    writeLE32(image.data() + 20, static_cast<uint32_t>(raw.size()));
    std::memcpy(image.data() + 0x30, compressed.data(), compressed.size());
    return image;
}

} // namespace

int main() {
    const std::vector<uint8_t> image = makeImage();

    // --- parser level -------------------------------------------------------
    rt8::FirmwareBank bank;
    std::string error;
    expect(rt8::parseFirmwareBytes(image.data(), image.size(), bank, error),
           "synthetic INIT image parses");
    if (!error.empty()) std::printf("parser error: %s\n", error.c_str());

    expect(bank.imported, "parsed bank is marked as imported firmware");
    expect(std::string(bank.tones[0].name) == "PCM SNARE", "tone name is taken from the image");
    expect(bank.tones[0].category == 6, "image tone category is preserved");
    expect(bank.tones[0].engineType == 2, "image tone engine type is preserved");
    expect(std::string(bank.tones[50].name) == "707 Cowbell", "missing tone name falls back to metadata");
    expect(bank.tones[50].category == 11, "metadata fallback also fixes the category");
    expect(std::string(bank.tones[200].name) == "Tone 200", "unnamed PCM tone gets a numbered name");
    expect(bank.tones[200].category == 1, "unnamed PCM tone infers a category from its id");

    expect(std::string(bank.kits[0].name) == "SYNTH KIT", "kit name is parsed");
    expect(bank.kits[0].channels[0].decay == 128, "PCM decay 255 normalises to the centre");
    expect(bank.kits[0].channels[0].revSend == 200, "full kit reverb master keeps the send");
    expect(bank.kits[0].channels[0].dlySend == 0, "zero kit delay master mutes the send");
    expect(bank.kits[0].channels[0].ctrl == 128, "filter-flag CTRL is centred");
    expect(bank.kits[0].channels[1].ctrl == 90, "real CTRL values are preserved");
    expect(bank.kits[0].channels[1].decay == 200, "ACB decay values are preserved");
    expect(bank.kits[1].channels[0].revSend == 0, "zero kit reverb master mutes the send");
    expect(bank.kits[1].channels[0].dlySend == 180, "full kit delay master keeps the send");

    // Corrupt images must be rejected, not half-loaded.
    {
        rt8::FirmwareBank rejected;
        std::string rejectError;
        const std::vector<uint8_t> truncated(image.begin(), image.begin() + 64);
        expect(!rt8::parseFirmwareBytes(truncated.data(), truncated.size(), rejected, rejectError),
               "truncated image is rejected");
        expect(!rejected.loaded, "rejected bank stays empty");
    }

    // --- engine level -------------------------------------------------------
    rt8::Engine engine;
    engine.prepare(48000.0, 512);
    std::string loadError;
    expect(engine.loadFirmware(image.data(), image.size(), loadError), "engine loads the image");
    expect(engine.isFirmwareLoaded(), "engine reports imported firmware");
    expect(engine.kitName(0) == "SYNTH KIT", "engine exposes the parsed kit name");
    expect(engine.toneName(50) == "707 Cowbell", "engine exposes the fallback tone name");

    engine.loadKit(0);
    expect(near(engine.getChannelParam(0, rt8::kReverbSend), 200.0f / 255.0f),
           "engine applies the scaled reverb send");
    expect(near(engine.getChannelParam(0, rt8::kDelaySend), 0.0f),
           "engine applies the muted delay send");
    expect(near(engine.getChannelParam(1, rt8::kCtrl), 90.0f / 255.0f),
           "engine applies the preserved CTRL value");

    engine.loadKit(1);
    expect(near(engine.getChannelParam(0, rt8::kReverbSend), 0.0f),
           "engine mutes reverb for the dry kit");
    expect(near(engine.getChannelParam(0, rt8::kDelaySend), 180.0f / 255.0f),
           "engine applies the alternate delay master");

    engine.loadKit(0);
    // The synthetic image has no pattern data, so play the pads directly.
    engine.trigger(0, 1.0f);
    std::vector<float> block(512 * 2, 0.0f);
    float peak = 0.0f;
    for (int i = 0; i < 96; ++i) {
        engine.process(block.data(), 512, 0);
        for (float v : block) {
            if (!std::isfinite(v)) {
                expect(false, "synthetic firmware renders finite samples");
                i = 96;
                break;
            }
            peak = std::max(peak, std::abs(v));
        }
    }
    expect(peak > 0.001f, "synthetic firmware produces audio");

    if (failures == 0) {
        std::printf("firmware_synthetic: all checks passed (peak %.3f)\n", peak);
        return 0;
    }
    std::fprintf(stderr, "firmware_synthetic: %d failure(s)\n", failures);
    return 1;
}
