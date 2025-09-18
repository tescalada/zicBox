#pragma once

#include <cmath>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <sndfile.h>

#include "plugins/audio/MultiEngine/Engine.h"
#include "helpers/range.h"

/*md
## PhysicalPianoEngine

Hybrid plucked-string style sustain using two slightly detuned waveguides to emulate
multi-string coupling. This is a lightweight scaffold toward a piano-like tone.
Attack is synthesized (noise burst shaped) for now; later we can swap to sampled attacks.
*/
class PhysicalPianoEngine : public Engine {
protected:
    struct WG {
        std::vector<float> delay;
        int idx = 0;
        int len = 64;
        float last = 0.0f;
        float ap_z = 0.0f; // allpass state
    } wgA, wgB, wgC;

    bool active = false;

    // Parameters mapped from UI (0..100%)
    float baseLoss = 0.9965f;   // DECAY base loss per sample
    float tone = 0.6f;          // BRIGHTNESS
    float detuneCents = 4.f;    // DETUNE (cents)
    float bodyMix = 0.2f;       // BODY (simple coloring)
    float sustainAmt = 0.0f;    // SUSTAIN (hold tail)
    float hardness = 0.5f;      // derived from BRIGHTNESS

    struct AttackSample {
        std::vector<float> data;
        int count = 0;
    } atk[4];
    int atkZone = -1;
    int atkIndex = 0;

    inline float noise()
    {
        return (float(rand()) / float(RAND_MAX)) * 2.0f - 1.0f;
    }

    bool loadAttackFile(const char* path, AttackSample& out)
    {
        SF_INFO sfinfo{};
        SNDFILE* f = sf_open(path, SFM_READ, &sfinfo);
        if (!f || sfinfo.frames <= 0) {
            return false;
        }
        std::vector<float> tmp(sfinfo.frames * sfinfo.channels);
        sf_read_float(f, tmp.data(), tmp.size());
        sf_close(f);
        // Convert to mono if needed (simple average)
        out.data.resize(sfinfo.frames);
        if (sfinfo.channels == 1) {
            std::copy(tmp.begin(), tmp.begin() + sfinfo.frames, out.data.begin());
        } else {
            for (int i = 0; i < sfinfo.frames; ++i) {
                float s = 0.0f;
                for (int c = 0; c < sfinfo.channels; ++c) s += tmp[i * sfinfo.channels + c];
                out.data[i] = s / sfinfo.channels;
            }
        }
        out.count = (int)out.data.size();
        return true;
    }

    void loadAttack(const char* base, AttackSample& out)
    {
        // Try WAV then AIFF
        std::string wav = std::string(base) + ".wav";
        std::string aif = std::string(base) + ".aiff";
        if (loadAttackFile(wav.c_str(), out)) return;
        loadAttackFile(aif.c_str(), out);
    }

    void ensureLen(WG& wg, float freq)
    {
        freq = std::clamp(freq, 10.0f, 20000.0f);
        int L = std::max(2, int(props.sampleRate / freq));
        if (L != wg.len) {
            wg.len = L;
            wg.delay.assign(wg.len, 0.0f);
            wg.idx = 0;
            wg.last = 0.0f;
            wg.ap_z = 0.0f;
        }
    }

    inline float freqDependentLoss(float freq)
    {
        float f = std::min(freq, 5000.0f) / 5000.0f; // 0..1
        float l = baseLoss - 0.01f * f * (1.0f - baseLoss);
        return std::clamp(l, 0.96f, 0.99999f);
    }

    float stepWG(WG& wg, float freq)
    {
        float current = wg.delay[wg.idx];
        float avg = 0.5f * (current + wg.delay[(wg.idx + wg.len - 1) % wg.len]);
        float filtered = tone * avg + (1.0f - tone) * wg.last;
        filtered *= freqDependentLoss(freq);

        // First-order allpass for dispersion
        float a = 0.05f + 0.15f * tone; // 0.05..0.2
        float ap_y = -a * filtered + wg.ap_z;
        wg.ap_z = filtered + a * ap_y;
        filtered = ap_y;
        wg.delay[wg.idx] = filtered;
        wg.idx = (wg.idx + 1) % wg.len;
        wg.last = filtered;
        return current;
    }

public:
    // Exposed values
    Val& decay = val(75.0f, "DECAY", { "Decay", .unit = "%" }, [&](auto p) {
        p.val.setFloat(p.value);
        float t = p.val.pct();
        baseLoss = 0.985f + t * (0.99995f - 0.985f);
    });

    Val& brightnessVal = val(60.0f, "BRIGHTNESS", { "Brightness", .unit = "%" }, [&](auto p) {
        p.val.setFloat(p.value);
        tone = p.val.pct();
        hardness = 0.2f + 0.8f * p.val.pct();
    });

    Val& detuneVal = val(40.0f, "DETUNE", { "Detune", .unit = "%" }, [&](auto p) {
        p.val.setFloat(p.value);
        // Map 0..100% to 0..12 cents
        detuneCents = p.val.pct() * 12.0f;
    });

    Val& bodyVal = val(20.0f, "BODY", { "Body", .unit = "%" }, [&](auto p) {
        p.val.setFloat(p.value);
        bodyMix = p.val.pct();
    });

    Val& sustainVal = val(0.0f, "SUSTAIN", { "Sustain", .unit = "%" }, [&](auto p) {
        p.val.setFloat(p.value);
        sustainAmt = p.val.pct();
    });

    PhysicalPianoEngine(AudioPlugin::Props& props, AudioPlugin::Config& config)
        : Engine(props, config, "Piano")
    {
        initValues();
        // Try load attacks per zone (optional)
        loadAttack("data/audio/samples/piano_attacks/zone_A1", atk[0]);
        loadAttack("data/audio/samples/piano_attacks/zone_C3", atk[1]);
        loadAttack("data/audio/samples/piano_attacks/zone_F4", atk[2]);
        loadAttack("data/audio/samples/piano_attacks/zone_C6", atk[3]);
    }

    void sample(float* buf, float envAmp) override
    {
        // Avoid stepping uninitialized buffers or when inactive
        if (!active || wgA.delay.empty() || wgB.delay.empty() || wgA.len <= 1 || wgB.len <= 1) {
            return;
        }

        float fA = props.sampleRate / std::max(2, wgA.len);
        float fB = props.sampleRate / std::max(2, wgB.len);
        float fC = wgC.len > 1 ? props.sampleRate / std::max(2, wgC.len) : fB;

        float a = stepWG(wgA, fA);
        float b = stepWG(wgB, fB);
        float c = wgC.delay.empty() ? 0.0f : stepWG(wgC, fC);

        float sustainOut = (a + b + c) / 3.0f;

        // Mix attack sample if available (pre-body, independent of envAmp)
        float attackOut = 0.0f;
        if (atkZone >= 0 && atkZone < 4) {
            AttackSample& as = atk[atkZone];
            // Limit to ~120ms
            int maxAtk = (int)(props.sampleRate * 0.12f);
            if (atkIndex < as.count && atkIndex < maxAtk) {
                float t = (float)atkIndex / (float)maxAtk;
                float g = 1.0f - t; // fade-out
                attackOut = as.data[atkIndex++] * g;
            }
        }

        // Body coloration (very light)
        float bodyLP = (sustainOut + wgA.last + wgB.last) * 0.3333f;
        sustainOut = (1.0f - bodyMix) * sustainOut + bodyMix * bodyLP;

        // Sustain: if knob high, bias envAmp upward to keep tail audible
        float sustainGain = 1.0f + 0.5f * sustainAmt;
        // Attack gain modest
        float attackGain = 0.9f;
        buf[track] = buf[track] + (sustainOut * envAmp * sustainGain) + (attackOut * attackGain);
    }

    void noteOn(uint8_t note, float velocity, void* userdata = nullptr) override
    {
        // Target fundamental
        float f0 = getMidiNoteFrequency(note);
        // Three strings: center, +detune, -detune*0.8
        float r1 = pow(2.0f, ( detuneCents / 1200.0f));
        float r2 = pow(2.0f, (-detuneCents * 0.8f / 1200.0f));
        ensureLen(wgA, f0);
        ensureLen(wgB, f0 * r1);
        ensureLen(wgC, f0 * r2);

        // Attack: shaped burst
        float scale = std::clamp(velocity, 0.0f, 1.0f);
        auto excite = [&](WG& wg, float gain) {
            for (int i = 0; i < wg.len; ++i) {
                float t = float(i) / wg.len;
                float env = powf(1.0f - t, 2.0f + 4.0f * (1.0f - hardness));
                float n = noise();
                float prev = (i ? wg.delay[i - 1] : 0.0f);
                float shaped = tanhf((0.8f + 1.5f * hardness) * (0.6f * n + 0.4f * (n + 0.5f * prev)));
                wg.delay[i] = shaped * env * scale * gain;
            }
        };
        excite(wgA, 1.0f);
        excite(wgB, 0.95f);
        excite(wgC, 0.9f);
        wgA.idx = 0; wgA.last = 0.0f; wgA.ap_z = 0.0f;
        wgB.idx = 0; wgB.last = 0.0f; wgB.ap_z = 0.0f;
        wgC.idx = 0; wgC.last = 0.0f; wgC.ap_z = 0.0f;
        // Choose attack zone by note (A1=33, C3=48, F4=65, C6=84)
        atkZone = (note < 48) ? 0 : (note < 65) ? 1 : (note < 84) ? 2 : 3;
        atkIndex = 0;
        active = true;
    }

    void noteOff(uint8_t, float, void* userdata = nullptr) override
    {
        // Natural decay
    }
};


