#pragma once

#include <Audio.h>
#include <Wire.h>
#include "audio/effect_dynamics.h"

// --- Audio-related functions accessible from elsewhere in the O_C codebase
namespace OC {
  namespace AudioDSP {

    // per channel
    enum ParamTarget {
      AMP_LEVEL,
      OSC_PITCH,
      OSC_SHAPE,
      FILTER_CUTOFF,
      FILTER_RESONANCE,
      WAVEFOLD_MOD,
      WAV_LEVEL,
      WAV_RATE,
      REVERB_LEVEL,
      REVERB_SIZE,
      REVERB_DAMP,

      TARGET_COUNT
    };

    enum ChannelSetting {
      PASSTHRU,
      OSCILLATOR,
      VCA_MODE,
      VCF_MODE,
      WAVEFOLDER,
      WAV_PLAYER,
      LOOP_LENGTH,
      WAV_PLAYER_VCA,
      WAV_PLAYER_RATE,

      MODE_COUNT
    };
    extern const char * const mode_names[];

    const int MAX_CV = 9216; // 6V

    extern bool wavplayer_available;
    extern uint8_t mods_enabled; // DAC outputs bitmask
    extern int mod_map[2][TARGET_COUNT]; // CV modulation sources (as channel indexes for [inputs..outputs])
    extern float bias[2][TARGET_COUNT]; // baseline settings
    extern ChannelSetting audio_cursor[2];
    extern bool isEditing[2];
    extern bool filter_enabled[2];
    extern uint8_t loop_length[2];
    extern bool loop_on[2];

    void Init();
    void Process(const int *values);
    void AudioMenuAdjust(int ch, int direction);
    void DrawAudioSetup();
    bool FileIsPlaying(int ch);
    uint8_t GetFileNum(int ch);
    uint32_t GetFileTime(int ch);
    uint16_t GetFileBPM(int ch);
    void FileMatchTempo(int ch);
    void AudioSetupAuxButton(int ch);

    static inline void AudioSetupButtonAction(int ch) {
      isEditing[ch] = !isEditing[ch];
    }
  } // AudioDSP namespace
} // OC namespace
