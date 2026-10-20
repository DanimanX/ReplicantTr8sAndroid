#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

#include "dsp_common.h"

namespace rt8 {

// Freeverb-style stereo reverb, tuned for a dense, dark drum-room tail.
class Reverb {
public:
    void prepare(double sampleRate) {
        sampleRate_ = sampleRate < 8000.0 ? 8000.0 : sampleRate;
        const double scale = sampleRate_ / 44100.0;
        static constexpr int combTuning[8] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
        static constexpr int allpassTuning[4] = {556, 441, 341, 225};
        for (int i = 0; i < 8; ++i) {
            combL_[i].resize(static_cast<int>(combTuning[i] * scale));
            combR_[i].resize(static_cast<int>((combTuning[i] + 23) * scale));
        }
        for (int i = 0; i < 4; ++i) {
            allpassL_[i].resize(static_cast<int>(allpassTuning[i] * scale));
            allpassR_[i].resize(static_cast<int>((allpassTuning[i] + 23) * scale));
        }
        reset();
    }

    void reset() {
        for (auto& c : combL_) c.reset();
        for (auto& c : combR_) c.reset();
        for (auto& a : allpassL_) a.reset();
        for (auto& a : allpassR_) a.reset();
    }

    void setParameters(float roomSize, float damping, float width) {
        feedback_ = 0.7f + roomSize * 0.28f;
        damping_ = damping * 0.4f;
        wet1_ = width * 0.5f + 0.5f;
        wet2_ = (1.0f - width) * 0.5f;
    }

    void process(float& left, float& right) {
        const float input = (left + right) * 0.015f;
        float outL = 0.0f;
        float outR = 0.0f;
        for (int i = 0; i < 8; ++i) {
            outL += combL_[i].process(input, feedback_, damping_);
            outR += combR_[i].process(input, feedback_, damping_);
        }
        for (int i = 0; i < 4; ++i) {
            outL = allpassL_[i].process(outL);
            outR = allpassR_[i].process(outR);
        }
        left = outL * wet1_ + outR * wet2_;
        right = outR * wet1_ + outL * wet2_;
    }

private:
    struct Comb {
        std::vector<float> buffer;
        size_t index = 0;
        float store = 0.0f;

        void resize(int size) { buffer.assign(static_cast<size_t>(size < 4 ? 4 : size), 0.0f); }
        void reset() {
            std::fill(buffer.begin(), buffer.end(), 0.0f);
            index = 0;
            store = 0.0f;
        }
        float process(float input, float feedback, float damping) {
            const float output = buffer[index];
            store = flushDenormal(output + (store - output) * damping);
            buffer[index] = flushDenormal(input + store * feedback);
            if (++index >= buffer.size()) index = 0;
            return output;
        }
    };

    struct Allpass {
        std::vector<float> buffer;
        size_t index = 0;

        void resize(int size) { buffer.assign(static_cast<size_t>(size < 4 ? 4 : size), 0.0f); }
        void reset() {
            std::fill(buffer.begin(), buffer.end(), 0.0f);
            index = 0;
        }
        float process(float input) {
            const float buffered = buffer[index];
            const float output = buffered - input;
            buffer[index] = flushDenormal(input + buffered * 0.5f);
            if (++index >= buffer.size()) index = 0;
            return output;
        }
    };

    double sampleRate_ = 48000.0;
    float feedback_ = 0.88f;
    float damping_ = 0.14f;
    float wet1_ = 0.98f;
    float wet2_ = 0.02f;

    std::array<Comb, 8> combL_ {};
    std::array<Comb, 8> combR_ {};
    std::array<Allpass, 4> allpassL_ {};
    std::array<Allpass, 4> allpassR_ {};
};

} // namespace rt8
