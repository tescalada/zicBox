#pragma once

#include "AudioAlsa.h"
#include <errno.h>

class AudioOutputAlsa_int16 : public AudioAlsa {
public:
    AudioOutputAlsa_int16(AudioPlugin::Props& props, AudioPlugin::Config& config)
        : AudioAlsa(props, config, SND_PCM_STREAM_PLAYBACK)
    {
        open(SND_PCM_FORMAT_S16_LE);
    }

    int16_t bufferInt16[audioChunk * ALSA_MAX_CHANNELS];
    void sample(float* buf) override
    {
        if (bufferIndex >= bufferSize) {
            bufferIndex = 0;
            if (handle) {
                // Robust write with recovery and partial writes handling
                unsigned int channels = bufferSize / audioChunk;
                snd_pcm_sframes_t framesToWrite = audioChunk;
                snd_pcm_sframes_t framesWritten = 0;
                while (framesToWrite > 0) {
                    int16_t* ptr = bufferInt16 + (framesWritten * channels);
                    snd_pcm_sframes_t rc = snd_pcm_writei(handle, ptr, framesToWrite);
                    if (rc < 0) {
                        snd_pcm_sframes_t rcv = snd_pcm_recover(handle, rc, 1);
                        if (rcv < 0) {
                            logError("ALSA int16 recover failed: %ld\n", (long)rcv);
                            break;
                        }
                        logWarn("ALSA int16 recovered from %ld\n", (long)rc);
                        continue; // recovered, retry write
                    }
                    framesWritten += rc;
                    framesToWrite -= rc;
                }
            }
        }
        // Convert the single float sample to a 16-bit integer
        bufferInt16[bufferIndex++] = static_cast<int16_t>(buf[track] * 32767.0f);
    }
};
