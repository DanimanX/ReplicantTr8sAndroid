#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "engine.h"

namespace {

void writeWav(const char* path, const std::vector<float>& interleaved, int sampleRate) {
    const uint32_t frames = static_cast<uint32_t>(interleaved.size() / 2);
    const uint32_t dataBytes = frames * 2 * 2;
    FILE* f = std::fopen(path, "wb");
    if (f == nullptr) {
        std::perror(path);
        return;
    }
    auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
    auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
    std::fwrite("RIFF", 1, 4, f);
    u32(36 + dataBytes);
    std::fwrite("WAVEfmt ", 1, 8, f);
    u32(16);
    u16(1);
    u16(2);
    u32(static_cast<uint32_t>(sampleRate));
    u32(static_cast<uint32_t>(sampleRate * 4));
    u16(4);
    u16(16);
    std::fwrite("data", 1, 4, f);
    u32(dataBytes);
    for (float v : interleaved) {
        const float clamped = std::max(-1.0f, std::min(1.0f, v));
        const auto s = static_cast<int16_t>(std::lround(clamped * 32767.0f));
        u16(static_cast<uint16_t>(s));
    }
    std::fclose(f);
}

} // namespace

int main(int argc, char** argv) {
    const char* out = argc > 1 ? argv[1] : "replicant_tr8s_android_demo.wav";
    constexpr int kSampleRate = 48000;
    constexpr int kFrames = 512;
    constexpr double kDuration = 48.0;

    rt8::Engine engine;
    engine.prepare(kSampleRate, kFrames);
    engine.setMasterParam(rt8::kMasterVolume, 0.88f);
    engine.setMasterParam(rt8::kMasterReverb, 0.45f);
    engine.setMasterParam(rt8::kMasterDelay, 0.32f);
    engine.setMasterParam(rt8::kMasterDrive, 0.18f);
    engine.setScatterActive(false);

    struct Section {
        double start;
        int pattern;
        bool loadKit;
        bool fill;
        bool scatter;
    };
    const Section sections[] = {
        {0.0, 0, true, false, false},
        {8.0, 6, true, false, false},
        {16.0, 3, true, true, false},
        {22.0, 11, true, false, true},
        {30.0, 1, true, false, false},
        {38.0, 4, true, false, false},
        {42.0, 5, true, false, false},
    };

    std::vector<float> interleaved;
    interleaved.reserve(static_cast<size_t>(kDuration * kSampleRate) * 2);

    int sectionIndex = 0;
    engine.loadPattern(sections[0].pattern, true);
    engine.setPlaying(true);

    const int totalFrames = static_cast<int>(kDuration * kSampleRate);
    std::vector<float> block(static_cast<size_t>(kFrames) * 2, 0.0f);
    int frame = 0;
    while (frame < totalFrames) {
        const double t = static_cast<double>(frame) / kSampleRate;
        while (sectionIndex + 1 < static_cast<int>(sizeof(sections) / sizeof(sections[0])) &&
               t >= sections[sectionIndex + 1].start) {
            ++sectionIndex;
            engine.loadPattern(sections[sectionIndex].pattern, sections[sectionIndex].loadKit);
            engine.setFill(sections[sectionIndex].fill);
            engine.setScatterActive(sections[sectionIndex].scatter);
        }

        // Performance gestures: fill every 4th bar, scatter bursts, filter sweeps.
        const int bar = static_cast<int>(t / (4.0 * 60.0 / 138.0));
        if ((bar % 4) == 3 && !engine.isFill()) engine.toggleFill();
        if (t > 20.0 && t < 22.0) engine.setScatterType(3 + (bar % 3));
        if (t > 34.0 && t < 38.0) {
            const float sweep = static_cast<float>((t - 34.0) / 4.0);
            engine.setMasterParam(rt8::kMasterFilter, 0.5f - sweep * 0.35f);
        } else {
            engine.setMasterParam(rt8::kMasterFilter, 0.5f);
        }

        const int n = std::min(kFrames, totalFrames - frame);
        engine.process(block.data(), n, 0);
        interleaved.insert(interleaved.end(), block.begin(), block.begin() + n * 2);
        frame += n;
    }

    // Normalize to -1 dBFS.
    float peak = 0.0f;
    for (float v : interleaved) peak = std::max(peak, std::abs(v));
    if (peak > 0.0001f) {
        const float gain = 0.89f / peak;
        for (float& v : interleaved) v *= gain;
    }
    writeWav(out, interleaved, kSampleRate);
    std::printf("render_demo: wrote %s (%.1fs, peak %.2f)\n", out, kDuration, peak);
    return 0;
}
