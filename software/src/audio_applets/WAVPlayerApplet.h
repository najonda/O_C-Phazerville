#include "Audio/AudioPassthrough.h"
#include "HemisphereAudioApplet.h"
#include <TeensyVariablePlayback.h>

template <AudioChannels Channels>
class WavPlayerApplet : public HemisphereAudioApplet {
public:
  const char* applet_name() {
    return "WavPlay";
  }

  void Start() {
    for (int i = 0; i < Channels; i++) {
      in_conns[i].connect(input, i, mixer[i], 3);
      out_conns[i].connect(mixer[i], 0, output, i);
    }

    hpfilter[0][0].resonance(0.7);
    hpfilter[0][1].resonance(0.7);
    hpfilter[1][0].resonance(0.7);
    hpfilter[1][1].resonance(0.7);
    FileHPF(0, 0);
    FileHPF(1, 0);

    // -- SD card WAV players
    if (!HS::wavplayer_available) {
      Serial.println("Unable to access the SD card");
    }
    else {
      wavplayer[0].enableInterpolation(true);
      wavplayer[1].enableInterpolation(true);
      wavplayer[0].setBufferInPSRAM(true);
      wavplayer[1].setBufferInPSRAM(true);
    }
  }

  void Controller() {
    float gain = 0.01f
      * (level * static_cast<float>(level_cv.In(HEMISPHERE_MAX_INPUT_CV))
           / HEMISPHERE_MAX_INPUT_CV
         + offset);
    for (int i = 0; i < Channels; i++) {
      mixer[i].gain(0, gain);

      const int speed_cv = 0; // TODO:
      if (speed_cv > 0)
        FileRate(i, speed_cv);
      else
        FileMatchTempo(i);

      if (HS::clock_m.EndOfBeat()) {
        if (loop_length[i] && loop_on[i]) {
          if (++loop_count[i] >= loop_length[i])
            FileHotCue(i);
        }

        wavplayer[i].syncTrig();
      }
    }
  }

  // TODO: call this from somewhere....
  void mainloop() {
    if (HS::wavplayer_available) {
      for (int ch = 0; ch < 2; ++ch) {
        if (wavplayer_reload[ch]) {
          FileLoad(ch);
          wavplayer_reload[ch] = false;
        }

        if (wavplayer_playtrig[ch]) {
          wavplayer[ch].play();
          wavplayer_playtrig[ch] = false;
        }
      }
    }
  }

  void View() {
    // TODO: play/stop button; file select; loop; HPF

    gfxPrint(1, 15, "Lvl:");
    gfxStartCursor();
    graphics.printf("%4d%%", level);
    gfxEndCursor(cursor == 0);
    gfxStartCursor();
    gfxPrintIcon(level_cv.Icon());
    gfxEndCursor(cursor == 1);
  }

  void OnEncoderMove(int direction) {
    if (!EditMode()) {
      MoveCursor(cursor, direction, NUM_PARAMS - 1);
      return;
    }
    switch (cursor) {
      case 0:
        level = constrain(level + direction, -200, 200);
        break;
      case 1:
        level_cv.ChangeSource(direction);
        break;
    }
  }

  uint64_t OnDataRequest() {
    return 0;
  }
  void OnDataReceive(uint64_t data) {}

  AudioStream* InputStream() override {
    return &input;
  }
  AudioStream* OutputStream() override {
    return &output;
  }

protected:
  void SetHelp() override {}

private:
  const int NUM_PARAMS = 1;
  int cursor = 0;
  int level = 100;
  CVInput level_cv;
  int offset = 0;
  int shape = 0;
  CVInput shape_cv;

  AudioPassthrough<Channels> input;
  AudioPlaySdResmp      wavplayer[2];
  AudioFilterStateVariable hpfilter[2][2];
  AudioMixer4           mixer[2];
  AudioPassthrough<Channels> output;

  std::array<AudioConnection, Channels> in_conns;
  std::array<AudioConnection, Channels> out_conns;

  AudioConnection          patchCordWav1L{wavplayer[0], 0, hpfilter[0][0], 0};
  AudioConnection          patchCordWav1R{wavplayer[0], 1, hpfilter[0][1], 0};
  AudioConnection          patchCordWav2L{wavplayer[1], 0, hpfilter[1][0], 0};
  AudioConnection          patchCordWav2R{wavplayer[1], 1, hpfilter[1][1], 0};
  AudioConnection          patchCordWavHPF1L{hpfilter[0][0], 2, mixer[0], 0};
  AudioConnection          patchCordWavHPF1R{hpfilter[0][1], 2, mixer[1], 0};
  AudioConnection          patchCordWavHPF2L{hpfilter[1][0], 2, mixer[0], 1};
  AudioConnection          patchCordWavHPF2R{hpfilter[1][1], 2, mixer[1], 1};

  // SD player vars, copied from other dev branch
  bool wavplayer_reload[2] = {true, true};
  bool wavplayer_playtrig[2] = {false};
  uint8_t wavplayer_select[2] = { 1, 10 };
  float wavlevel[2] = { 1.0, 1.0 };
  uint8_t loop_length[2] = { 8, 8 };
  int8_t loop_count[2] = { 0, 0 };
  bool loop_on[2] = { false, false };
  bool lowcut[2] = { false, false };

  // SD file player functions
  void FileLoad(int ch) {
    char filename[] = "000.WAV";
    filename[1] += wavplayer_select[ch] / 10;
    filename[2] += wavplayer_select[ch] % 10;
    wavplayer[ch].playWav(filename);
  }
  void StartPlaying(int ch) {
    wavplayer_playtrig[ch] = true;
    loop_count[ch] = -1;
  }
  bool FileIsPlaying(int ch = 0) {
    return wavplayer[ch].isPlaying();
  }
  void ToggleFilePlayer(int ch = 0) {
    if (wavplayer[ch].isPlaying()) {
      wavplayer[ch].stop();
    } else if (HS::wavplayer_available) {
      StartPlaying(ch);
    }
  }
  void FileHotCue(int ch) {
    if (wavplayer[ch].available()) {
      wavplayer[ch].retrigger();
      loop_count[ch] = 0;
    }
  }
  void ToggleLoop(int ch) {
    if (loop_length[ch] && !loop_on[ch]) {
      const uint32_t start = wavplayer[ch].isPlaying() ?
                    wavplayer[ch].getPosition() : 0;
      wavplayer[ch].setLoopStart( start );
      wavplayer[ch].setPlayStart(play_start_loop);
      if (wavplayer[ch].available())
        wavplayer[ch].retrigger();
      loop_on[ch] = true;
      loop_count[ch] = -1;
    } else {
      wavplayer[ch].setPlayStart(play_start_sample);
      loop_on[ch] = false;
    }
  }

  void FileHPF(int ch, int cv) {
    float freq = abs(cv) / 64;
    freq *= freq;

    hpfilter[ch][0].frequency(freq);
    hpfilter[ch][1].frequency(freq);
  }
  void ToggleLowCut(int ch) {
    lowcut[ch] = !lowcut[ch];
    FileHPF(ch, lowcut[ch] * 1000);
  }

  // simple hooks for beat-sync callbacks
  void FileToggle1() { ToggleFilePlayer(0); }
  void FileToggle2() { ToggleFilePlayer(1); }
  void FilePlay1() { StartPlaying(0); }
  void FilePlay2() { StartPlaying(1); }
  void FileLoopToggle1() { ToggleLoop(0); }
  void FileLoopToggle2() { ToggleLoop(1); }

  void ChangeToFile(int ch, int select) {
    wavplayer_select[ch] = (uint8_t)constrain(select, 0, 99);
    wavplayer_reload[ch] = true;
    if (wavplayer[ch].isPlaying()) {
      if (HS::clock_m.IsRunning()) {
        HS::clock_m.BeatSync( ch ? &FilePlay2 : &FilePlay1 );
      } else
        StartPlaying(ch);
    }
  }
  uint8_t GetFileNum(int ch) {
    return wavplayer_select[ch];
  }
  uint32_t GetFileTime(int ch) {
    return wavplayer[ch].positionMillis();
  }
  uint16_t GetFileBPM(int ch) {
    return (uint16_t)wavplayer[ch].getBPM();
  }
  void FileMatchTempo(int ch) {
    wavplayer[ch].matchTempo(HS::clock_m.GetTempoFloat());
  }
  void FileLevel(int ch, int cv) {
    wavlevel[ch] = (float)cv / HEMISPHERE_MAX_CV;
    mixer[0].gain(1 + ch, 0.9 * wavlevel[ch]);
    mixer[1].gain(1 + ch, 0.9 * wavlevel[ch]);
  }
  void FileRate(int ch, int cv) {
    // bipolar CV has +/- 50% pitch bend
    wavplayer[ch].setPlaybackRate((float)cv / HEMISPHERE_MAX_CV * 0.5 + 1.0);
  }
};
