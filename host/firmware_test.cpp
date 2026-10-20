#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "engine.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: firmware_test <TR8S_UP.bin>\n");
        return 2;
    }
    std::ifstream input(argv[1], std::ios::binary);
    if (!input) {
        std::fprintf(stderr, "cannot open %s\n", argv[1]);
        return 2;
    }
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)),
                               std::istreambuf_iterator<char>());
    std::printf("firmware bytes: %zu\n", bytes.size());

    rt8::Engine engine;
    engine.prepare(48000.0, 512);
    std::string error;
    const bool ok = engine.loadFirmware(bytes.data(), bytes.size(), error);
    if (!ok) {
        std::fprintf(stderr, "parse failed: %s\n", error.c_str());
        return 1;
    }

    int failures = 0;
    auto check = [&](bool condition, const char* what) {
        if (!condition) {
            std::fprintf(stderr, "FAIL: %s\n", what);
            ++failures;
        }
    };

    check(engine.isFirmwareLoaded(), "firmware flag is set");
    std::printf("kit 001: %s\n", engine.kitName(0).c_str());
    std::printf("kit 002: %s\n", engine.kitName(1).c_str());
    std::printf("pattern 7-08: %s %s\n", engine.patternCode(103).c_str(),
                engine.patternName(103).c_str());
    std::printf("tone 027: %s\n", engine.toneName(27).c_str());
    check(engine.kitName(0) != "INIT KIT 001", "factory kit names parsed");
    check(engine.toneName(27) != "Tone 027", "factory tone names parsed");

    int namedKits = 0;
    for (int i = 0; i < 128; ++i) {
        const auto name = engine.kitName(i);
        if (name.find("INIT KIT") == std::string::npos) ++namedKits;
    }
    std::printf("named kits: %d\n", namedKits);
    check(namedKits > 100, "most factory kits have names");

    engine.loadPattern(103, true);
    engine.setPlaying(true);
    std::vector<float> block(512 * 2, 0.0f);
    float peak = 0.0f;
    for (int i = 0; i < 200; ++i) {
        engine.process(block.data(), 512, 0);
        for (float v : block) {
            if (!std::isfinite(v)) {
                check(false, "firmware factory pattern produced a non-finite sample");
                i = 1000;
                break;
            }
            peak = std::max(peak, std::abs(v));
        }
    }
    std::printf("factory pattern peak: %.3f\n", peak);
    check(peak > 0.01f, "factory pattern renders audio");

    if (failures != 0) {
        std::fprintf(stderr, "firmware_test: %d failure(s)\n", failures);
        return 1;
    }
    std::printf("firmware_test: all checks passed\n");
    return 0;
}
