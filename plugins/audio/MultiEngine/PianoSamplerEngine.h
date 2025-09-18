#pragma once

#include <cmath>
#include <map>
#include <string>
#include <vector>
#include <algorithm>

#include <sndfile.h>
#include "log.h"

#include "plugins/audio/MultiEngine/Engine.h"

/*md
## PianoSamplerEngine

Simple mono piano sampler that maps each MIDI note to a per-note piano sample
located under `data/audio/samples/piano/`.

Expected filenames (either .wav or .aiff):
- Piano.pp.C4.wav / Piano.pp.C4.aiff
with flats for black keys: Db, Eb, Gb, Ab, Bb.
*/
class PianoSamplerEngine : public Engine {
protected:
    struct Sample {
        std::vector<float> data;
        int count = 0;
        int sr = 44100;
        bool loaded = false;
    };

    Sample noteToSample[128];

    struct Voice {
        int note = -1;
        int playIndex = 0;     // integer position in sample frames
        float frac = 0.0f;     // fractional for resample
        float step = 1.0f;     // playback step (resample ratio)
        float gain = 1.0f;     // velocity * gain param
        const Sample* samp = nullptr;
        bool active = false;
        float lpState = 0.0f;  // per-voice tone filter state
    };

    static constexpr int MAX_VOICES = 8;
    Voice voices[MAX_VOICES];
    int voiceCursor = 0;

    // Global tone/gain
    float tone = 0.5f;    // 0..1 brightness
    float outGain = 1.0f; // overall gain

    // Global envelope is provided by SynthMulti; no per-voice ADSR here

    std::string noteNameFlats(int midi)
    {
        static const char* names[12] = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
        int n = midi % 12;
        int oct = midi / 12 - 1;
        char buf[8];
        snprintf(buf, sizeof(buf), "%s%d", names[n], oct);
        return std::string(buf);
    }

    bool tryLoad(const std::string& path, Sample& s)
    {
        SF_INFO info{};
        SNDFILE* f = sf_open(path.c_str(), SFM_READ, &info);
        if (!f) return false;
        std::vector<float> temp(info.frames * info.channels);
        sf_read_float(f, temp.data(), temp.size());
        sf_close(f);
        s.data.resize(info.frames);
        if (info.channels == 1) {
            std::copy(temp.begin(), temp.begin() + info.frames, s.data.begin());
        } else {
            for (int i = 0; i < info.frames; ++i) {
                float acc = 0.0f;
                for (int c = 0; c < info.channels; ++c) acc += temp[i * info.channels + c];
                s.data[i] = acc / info.channels;
            }
        }
        s.count = (int)s.data.size();
        s.sr = info.samplerate;
        s.loaded = true;
        logDebug("PianoSampler loaded %s (frames=%d, sr=%d)", path.c_str(), s.count, s.sr);
        return true;
    }

    void ensureSampleLoaded(int midi)
    {
        if (midi < 0 || midi > 127) return;
        Sample& s = noteToSample[midi];
        if (s.loaded) return;
        std::string nn = noteNameFlats(midi);
        std::string base = std::string("data/audio/samples/piano/Piano.pp.") + nn;
        if (tryLoad(base + ".wav", s)) return;
        if (tryLoad(base + ".aiff", s)) return;
        // Fallback: search nearest available note within +/- 12 semitones
        for (int off = 1; off <= 12; ++off) {
            int down = midi - off;
            int up = midi + off;
            if (down >= 0) {
                std::string dn = noteNameFlats(down);
                std::string db = std::string("data/audio/samples/piano/Piano.pp.") + dn;
                if (tryLoad(db + ".wav", s) || tryLoad(db + ".aiff", s)) {
                    logWarn("PianoSampler fallback: %s -> %s", nn.c_str(), dn.c_str());
                    return;
                }
            }
            if (up <= 127) {
                std::string un = noteNameFlats(up);
                std::string ub = std::string("data/audio/samples/piano/Piano.pp.") + un;
                if (tryLoad(ub + ".wav", s) || tryLoad(ub + ".aiff", s)) {
                    logWarn("PianoSampler fallback: %s -> %s", nn.c_str(), un.c_str());
                    return;
                }
            }
        }
        // fallback: silence
        s.data.assign(1, 0.0f);
        s.count = 1;
        s.sr = props.sampleRate;
        s.loaded = true;
        logWarn("PianoSampler missing sample for %s; using silence", nn.c_str());
    }

public:
    // Values (kept minimal, but useful)

    Val& brightnessVal = val(50.0f, "TONE", { "Tone", .unit = "%" }, [&](auto p) {
        p.val.setFloat(p.value);
        tone = p.val.pct();
    });

    Val& gainVal = val(80.0f, "GAIN", { "Gain", .unit = "%" }, [&](auto p) {
        p.val.setFloat(p.value);
        // Map 0..100% -> 0..1.5 linear
        outGain = 0.0f + 1.5f * p.val.pct();
    });

    PianoSamplerEngine(AudioPlugin::Props& props, AudioPlugin::Config& config)
        : Engine(props, config, "PianoSampler")
    {
        initValues();
    }

    void sample(float* buf, float envAmp) override
    {
        float sum = 0.0f;
        for (int i = 0; i < MAX_VOICES; ++i) {
            Voice& v = voices[i];
            if (!v.active || !v.samp) continue;
            const Sample& s = *v.samp;
            if (v.playIndex >= s.count) { v.active = false; continue; }

            // Resample (nearest + linear blend)
            float sampVal = 0.0f;
            if (s.sr == props.sampleRate) {
                int idx = std::min(v.playIndex, s.count - 1);
                sampVal = s.data[idx];
                v.playIndex++;
            } else {
                if (v.step == 1.0f) {
                    v.step = (float)s.sr / (float)props.sampleRate;
                }
                float pos = v.playIndex + v.frac;
                int i0 = (int)pos;
                int i1 = std::min(i0 + 1, s.count - 1);
                float t = pos - i0;
                float s0 = s.data[std::min(i0, s.count - 1)];
                float s1 = s.data[i1];
                sampVal = s0 + (s1 - s0) * t;
                pos += v.step;
                v.playIndex = (int)pos;
                v.frac = pos - v.playIndex;
            }

            // Tone filter per-voice
            float cutoff = 1000.0f + tone * 12000.0f; // 1k..13k
            float x = expf(-2.0f * 3.1415926f * cutoff / props.sampleRate);
            float alpha = 1.0f - x;
            v.lpState = v.lpState + alpha * (sampVal - v.lpState);
            float toned = (tone > 0.99f) ? sampVal : v.lpState;

            sum += toned * v.gain * envAmp;
        }
        buf[track] += sum * outGain;
    }

    void noteOn(uint8_t note, float velocity, void* userdata = nullptr) override
    {
        ensureSampleLoaded(note);
        // Find a voice to use (free or steal round-robin)
        int vi = -1;
        for (int i = 0; i < MAX_VOICES; ++i) {
            if (!voices[i].active) { vi = i; break; }
        }
        if (vi < 0) { vi = voiceCursor; voiceCursor = (voiceCursor + 1) % MAX_VOICES; }
        Voice& v = voices[vi];
        v.note = note;
        v.samp = &noteToSample[note];
        v.playIndex = 0;
        v.frac = 0.0f;
        v.step = (v.samp->sr == props.sampleRate) ? 1.0f : ((float)v.samp->sr / (float)props.sampleRate);
        v.gain = std::clamp(velocity, 0.0f, 1.0f);
        v.lpState = 0.0f;
        v.active = true;
    }

    void noteOff(uint8_t, float, void* userdata = nullptr) override
    {
        // Global envelope handles release; nothing to do per-voice
    }
};


