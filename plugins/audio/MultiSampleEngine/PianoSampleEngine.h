#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include <sndfile.h>

#include "plugins/audio/MultiSampleEngine/SampleEngine.h"
#include "log.h"

/*md
## PianoSampleEngine

Generic sampler engine for Sample tracks using per-note piano files.
Looks under `data/audio/samples/piano/` with filenames like `Piano.pp.C4.wav` (or .aiff).
Controls: ATTACK, DECAY, SUSTAIN, RELEASE, TONE, GAIN.
*/
class PianoSampleEngine : public SampleEngine {
protected:
    struct Sample {
        std::vector<float> data;
        int count = 0;
        int sr = 44100;
        bool loaded = false;
    } noteToSample[128];

    struct Voice {
        int note = -1;
        int idx = 0;
        float frac = 0.0f;
        float step = 1.0f;
        float env = 0.0f;
        uint8_t stage = 0; // 0=A,1=D,2=S,3=R
        float gain = 1.0f;
        float lp = 0.0f; // tone filter state
        const Sample* s = nullptr;
        bool active = false;
    } voices[8];
    int voiceCursor = 0;

    // Params
    float attackMs = 5.0f;
    float decayMs = 120.0f;
    float sustainLvl = 0.7f;
    float releaseMs = 300.0f;
    float tone = 0.5f;    // 0..1 (LP cutoff)
    float outGain = 1.0f; // linear

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
        logDebug("PianoSample loaded %s (frames=%d, sr=%d)", path.c_str(), s.count, s.sr);
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
        for (int off = 1; off <= 12; ++off) {
            int down = midi - off;
            int up = midi + off;
            if (down >= 0) {
                std::string db = std::string("data/audio/samples/piano/Piano.pp.") + noteNameFlats(down);
                if (tryLoad(db + ".wav", s) || tryLoad(db + ".aiff", s)) return;
            }
            if (up <= 127) {
                std::string ub = std::string("data/audio/samples/piano/Piano.pp.") + noteNameFlats(up);
                if (tryLoad(ub + ".wav", s) || tryLoad(ub + ".aiff", s)) return;
            }
        }
        s.data.assign(1, 0.0f); s.count = 1; s.sr = props.sampleRate; s.loaded = true;
        logWarn("PianoSample missing note %s -> silence", nn.c_str());
    }

    inline float msStep(float ms) const { return ms <= 0.0f ? 1.0f : (1.0f / (ms * 0.001f * props.sampleRate)); }

public:
    PianoSampleEngine(AudioPlugin::Props& props, AudioPlugin::Config& config, SampleBuffer& sb, float& idx, float& stepMul)
        : SampleEngine(props, config, sb, idx, stepMul, "Piano (Sample)")
    {
        // Controls
        val(5.0f, "ATTACK", { "Attack", .min = 0.0f, .max = 2000.0f, .step = 1.0f, .unit = "ms" }, [&](auto p) { p.val.setFloat(p.value); attackMs = p.val.get(); });
        val(120.0f, "DECAY", { "Decay", .min = 0.0f, .max = 4000.0f, .step = 5.0f, .unit = "ms" }, [&](auto p) { p.val.setFloat(p.value); decayMs = p.val.get(); });
        val(70.0f, "SUSTAIN", { "Sustain", .unit = "%" }, [&](auto p) { p.val.setFloat(p.value); sustainLvl = std::clamp(p.val.pct(), 0.0f, 1.0f); });
        val(300.0f, "RELEASE", { "Release", .min = 0.0f, .max = 6000.0f, .step = 5.0f, .unit = "ms" }, [&](auto p) { p.val.setFloat(p.value); releaseMs = p.val.get(); });
        val(50.0f, "TONE", { "Tone", .unit = "%" }, [&](auto p) { p.val.setFloat(p.value); tone = p.val.pct(); });
        val(80.0f, "GAIN", { "Gain", .unit = "%" }, [&](auto p) { p.val.setFloat(p.value); outGain = 1.5f * p.val.pct(); });
    }

    void opened() override { }

    void noteOn(uint8_t note, float velocity, void* userdata = nullptr) override
    {
        ensureSampleLoaded(note);
        // Pick a voice
        int vi = -1;
        for (int i = 0; i < 8; ++i) if (!voices[i].active) { vi = i; break; }
        if (vi < 0) { vi = voiceCursor; voiceCursor = (voiceCursor + 1) % 8; }
        Voice& v = voices[vi];
        v.note = note;
        v.s = &noteToSample[note];
        v.idx = 0; v.frac = 0.0f;
        v.step = (v.s->sr == props.sampleRate) ? 1.0f : ((float)v.s->sr / (float)props.sampleRate);
        v.env = 0.0f; v.stage = 0; v.active = true; v.lp = 0.0f;
        v.gain = std::clamp(velocity, 0.0f, 1.0f);
    }

    void noteOff(uint8_t note, float velocity, void* userdata = nullptr) override
    {
        for (int i = 0; i < 8; ++i) if (voices[i].active && voices[i].note == note) voices[i].stage = 3;
    }

    void sample(float* buf) override
    {
        float mix = 0.0f;
        float aStep = msStep(attackMs), dStep = msStep(decayMs), rStep = msStep(releaseMs);
        for (int i = 0; i < 8; ++i) {
            Voice& v = voices[i];
            if (!v.active || !v.s) continue;
            if (v.idx >= v.s->count) { v.stage = 3; }

            // ADSR
            if (v.stage == 0) { v.env += aStep; if (v.env >= 1.0f) { v.env = 1.0f; v.stage = 1; } }
            else if (v.stage == 1) { v.env -= dStep * (1.0f - sustainLvl); if (v.env <= sustainLvl) { v.env = sustainLvl; v.stage = 2; } }
            else if (v.stage == 3) { v.env -= rStep * std::max(v.env, 0.0001f); if (v.env <= 0.0f) { v.env = 0.0f; v.active = false; continue; } }

            // Resample
            float sVal = 0.0f;
            if (v.s->sr == props.sampleRate) {
                int ii = std::min(v.idx, v.s->count - 1);
                sVal = v.s->data[ii];
                v.idx++;
            } else {
                float pos = v.idx + v.frac;
                int i0 = (int)pos;
                int i1 = std::min(i0 + 1, v.s->count - 1);
                float t = pos - i0;
                float s0 = v.s->data[std::min(i0, v.s->count - 1)];
                float s1 = v.s->data[i1];
                sVal = s0 + (s1 - s0) * t;
                pos += v.step;
                v.idx = (int)pos; v.frac = pos - v.idx;
            }

            // Tone LP
            float cutoff = 1000.0f + tone * 12000.0f;
            float x = expf(-2.0f * 3.1415926f * cutoff / props.sampleRate);
            float alpha = 1.0f - x;
            v.lp = v.lp + alpha * (sVal - v.lp);
            float toned = (tone > 0.99f) ? sVal : v.lp;
            mix += toned * v.env * v.gain;
        }
        buf[track] += mix * outGain;
    }
};


