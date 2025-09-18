#pragma once

#include <cmath>
#include <cstdlib>
#include <vector>
#include <algorithm>

#include "plugins/audio/MultiEngine/Engine.h"
#include "helpers/range.h"

/*md
## PhysicalModelEngine

Lightweight Karplus–Strong plucked string/waveguide style engine.
Not a full piano model; useful for plucks/strings as a starting point.
*/
class PhysicalModelEngine : public Engine {
protected:
    std::vector<float> delayBuffer;
    int delayIndex = 0;
    int delayLength = 64;
    bool active = false;

    float lastOut = 0.0f;

    // Parameters mapped from UI values (0..100%)
    float lossPerSample = 0.996f;   // from DECAY
    float brightness = 0.5f;        // from BRIGHTNESS

    // Simple uniform noise
    inline float noise()
    {
        return (float(rand()) / float(RAND_MAX)) * 2.0f - 1.0f;
    }

public:
    // Values
    // Pitch offset in semitones around MIDI note (handled globally in SynthMulti via body param)
    Val& decay = val(70.0f, "DECAY", { "Decay", .unit = "%" }, [&](auto p) {
        p.val.setFloat(p.value);
        // Map 0..100% to loss per sample 0.98..0.9999
        float t = p.val.pct();
        lossPerSample = 0.98f + t * (0.9999f - 0.98f);
    });

    Val& brightnessVal = val(50.0f, "BRIGHTNESS", { "Brightness", .unit = "%" }, [&](auto p) {
        p.val.setFloat(p.value);
        brightness = p.val.pct();
    });

    PhysicalModelEngine(AudioPlugin::Props& props, AudioPlugin::Config& config)
        : Engine(props, config, "PhysicalModel")
    {
        initValues();
    }

    void ensureDelayForFreq(float freq)
    {
        freq = std::clamp(freq, 10.0f, 20000.0f);
        int len = std::max(2, int(props.sampleRate / freq));
        if (len != delayLength) {
            delayLength = len;
            delayBuffer.assign(delayLength, 0.0f);
            delayIndex = 0;
            lastOut = 0.0f;
        }
    }

    void sample(float* buf, float envAmp) override
    {
        float out = 0.0f;
        if (active && delayLength > 1) {
            // Read current sample
            float current = delayBuffer[delayIndex];

            // Simple 2-tap averaging (pick brightness between avg and previous output)
            float avg = 0.5f * (current + delayBuffer[(delayIndex + delayLength - 1) % delayLength]);
            float filtered = brightness * avg + (1.0f - brightness) * lastOut;

            // Loss
            filtered *= lossPerSample;

            // Write back and advance
            delayBuffer[delayIndex] = filtered;
            delayIndex = (delayIndex + 1) % delayLength;

            lastOut = filtered;
            out = current;

            // Auto-stop when energy decays
            if (std::fabs(filtered) < 1e-5f) {
                // keep running; noteOff manages active
            }
        }

        buf[track] = buf[track] + out * envAmp;
    }

    void noteOn(uint8_t note, float velocity, void* userdata = nullptr) override
    {
        // Convert MIDI note to frequency; include any pitch offset from Engine base
        float freq = getMidiNoteFrequency(note);
        ensureDelayForFreq(freq);

        // Excite string: noise burst scaled by velocity
        float scale = std::clamp(velocity, 0.0f, 1.0f);
        for (int i = 0; i < delayLength; ++i) {
            delayBuffer[i] = noise() * scale;
        }
        delayIndex = 0;
        lastOut = 0.0f;
        active = true;
    }

    void noteOff(uint8_t, float, void* userdata = nullptr) override
    {
        // Let it decay naturally; keep active true so tail outputs
    }
};


