#pragma once

#include "HemisphereApplet.h"
#include "dsputils.h"
#include <AudioStream.h>

const int NUM_CV_INPUTS = ADC_CHANNEL_LAST * 2 + 1;
// We *could* reuse HS::input_quant for inputs, but easier to just do it
// independently, and semitone quantizers are lightwieght.
OC::SemitoneQuantizer cv_semitone_quants[NUM_CV_INPUTS];

struct CVInput {
  int8_t source = 0;

  int In(int default_value = 0) {
    if (!source) return default_value;
    return source <= ADC_CHANNEL_LAST
      ? frame.inputs[source - 1]
      : frame.outputs[source - 1 - ADC_CHANNEL_LAST];
  }

  float InF(float default_value = 0.0f) {
    return In(static_cast<int>(default_value * HEMISPHERE_MAX_INPUT_CV))
      / static_cast<float>(HEMISPHERE_MAX_INPUT_CV);
  }

  int SemitoneIn(int default_value = 0) {
    return cv_semitone_quants[source].Process(In(default_value));
  }

  int InRescaled(int max_value) {
    return Proportion(In(), HEMISPHERE_MAX_INPUT_CV, max_value);
  }

  void ChangeSource(int dir) {
    source = constrain(source + dir, 0, NUM_CV_INPUTS - 1);
  }

  char const* InputName() const {
    return OC::Strings::cv_input_names_none[source];
  }

  uint8_t const* Icon() const {
    return PARAM_MAP_ICONS + 8 * source;
  }
};

struct DigitalInput {
  enum DigitalSourceType {
    NONE,
    CLOCK,
    DIGITAL_INPUT,
    CV_OUTPUT,
  };

  int8_t source = 0;
  bool last_gate_state = true; // for detecting clocks
  static const int ppqn = 4;
  static constexpr float internal_clocked_gate_pw = 0.5f;
  static const int num_sources = 2 + OC::DIGITAL_INPUT_LAST + ADC_CHANNEL_LAST;

  void ChangeSource(int dir) {
    source = constrain(source + dir, 0, num_sources);
  }

  bool Gate() {
    switch (source_type()) {
      case CLOCK: {
        uint32_t ticks_since_beat = OC::CORE::ticks - clock_m.beat_tick;
        uint32_t tick_phase = (ppqn * ticks_since_beat) % clock_m.ticks_per_beat;
        bool gate = tick_phase < internal_clocked_gate_pw * clock_m.ticks_per_beat;
        return gate;
      }
      case DIGITAL_INPUT:
        return frame.gate_high[digital_input_index()];
      case CV_OUTPUT:
        return frame.outputs[cv_output_index()] > GATE_THRESHOLD;
      case NONE:
      default:
        return false;
    }
  }

  /**
   * Returns to on rising gate input. Will return true once and then go back to
   * false until the gate goes low again.
   **/
  bool Clock() {
    bool gate = Gate();
    bool tock = !last_gate_state && gate;
    last_gate_state = gate;
    if (tock) serial_printf("tock %d\n", OC::CORE::ticks);
    return tock;
  }

  uint8_t const* Icon() const {
    switch (source_type()) {
      case CLOCK:
        return clock_m.cycle ? METRO_L_ICON : METRO_R_ICON;
      case DIGITAL_INPUT:
        return DIGITAL_INPUT_ICONS + digital_input_index() * 8;
      case CV_OUTPUT:
        return PARAM_MAP_ICONS + (1 + ADC_CHANNEL_LAST + cv_output_index()) * 8;
      case NONE:
      default:
        return PARAM_MAP_ICONS + 0;
    }
  }

private:
  DigitalSourceType source_type() const {
    switch (source) {
      case 0:
        return NONE;
      case 1:
        return CLOCK;
      default: {
        if (source < 2 + OC::DIGITAL_INPUT_LAST) {
          return DIGITAL_INPUT;
        } else {
          return CV_OUTPUT;
        }
      }
    }
  }

  inline int8_t digital_input_index() const {
    return source - 2;
  }

  inline int8_t cv_output_index() const {
    return source - 2 - OC::DIGITAL_INPUT_LAST;
  }
};

enum AudioChannels {
  NONE,
  MONO,
  STEREO,
};

class HemisphereAudioApplet : public HemisphereApplet {
public:
  virtual AudioStream* InputStream() = 0;
  virtual AudioStream* OutputStream() = 0;
  virtual void mainloop() {}

  void gfxPrintPitchHz(int16_t pitch, float base_freq = C3) {
    float freq = PitchToRatio(pitch) * base_freq;
    int int_part = static_cast<int>(freq);
    int dec = static_cast<int>(10 * (freq - int_part));
    graphics.printf("%5d.%01d", int_part, dec);
    gfxPrint("Hz");
  }
};
