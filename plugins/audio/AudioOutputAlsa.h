#pragma once

#include "AudioAlsa.h"
#include <errno.h>

/*md
## AudioOutputAlsa

AudioOutputAlsa plugin is used to write audio output to ALSA.

**Value**:
- `DEVICE: name` to set output device name. If not defined, default device will be used.
*/
class AudioOutputAlsa : public AudioAlsa {
public:
    AudioOutputAlsa(AudioPlugin::Props& props, AudioPlugin::Config& config)
        : AudioAlsa(props, config, SND_PCM_STREAM_PLAYBACK)
    {
        open();
    }

    void sample(float* buf) override
    {
        if (bufferIndex >= bufferSize) {
            bufferIndex = 0;
            if (handle) {
                // Robust write with short-write and error recovery
                unsigned int channels = bufferSize / audioChunk;
                snd_pcm_sframes_t framesToWrite = audioChunk;
                snd_pcm_sframes_t framesWritten = 0;
                while (framesToWrite > 0) {
                    float* ptr = buffer + (framesWritten * channels);
                    snd_pcm_sframes_t rc = snd_pcm_writei(handle, ptr, framesToWrite);
                    if (rc < 0) {
                        rc = snd_pcm_recover(handle, rc, 1);
                        if (rc < 0) {
                            logError("ALSA recover failed: %ld\n", (long)rc);
                            break;
                        }
                        logDebug("ALSA recover success: %ld\n", (long)rc);
                        continue; // recovered, retry write
                    }
                    framesWritten += rc;
                    framesToWrite -= rc;
                }
            }
        }
        buffer[bufferIndex++] = buf[track];
    }
};
