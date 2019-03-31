// Copyright 2019 Patrick Dowling
//
// Author: Patrick Dowling (pld@gurkenkiste.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// See http://creativecommons.org/licenses/MIT/ for more information.
//

#ifndef OC_IO_H_
#define OC_IO_H_

#include <array>

#include "OC_ADC.h"
#include "OC_DAC.h"
#include "OC_digital_inputs.h"
#include "util/util_settings.h"

namespace OC {

// Output value types
enum OutputMode {
  OUTPUT_MODE_PITCH,
  OUTPUT_MODE_GATE,
  OUTPUT_MODE_UNI,
  OUTPUT_MODE_RAW,
};

enum IO_SETTING {
  IO_SETTING_CV1_GAIN, IO_SETTING_CV1_FILTER, IO_SETTING_A_SCALING, IO_SETTING_A_TUNING,
  IO_SETTING_CV2_GAIN, IO_SETTING_CV2_FILTER, IO_SETTING_B_SCALING, IO_SETTING_B_TUNING,
  IO_SETTING_CV3_GAIN, IO_SETTING_CV3_FILTER, IO_SETTING_C_SCALING, IO_SETTING_C_TUNING,
  IO_SETTING_CV4_GAIN, IO_SETTING_CV4_FILTER, IO_SETTING_D_SCALING, IO_SETTING_D_TUNING,
  IO_SETTING_LAST
};

class IOSettings: public settings::SettingsBase<IOSettings, IO_SETTING_LAST> {
public:

  static inline IO_SETTING channel_setting(IO_SETTING setting, int channel) {
    return static_cast<IO_SETTING>(setting + channel * 4);
  }

  inline int input_gain(int channel) const {
    return get_value(channel_setting(IO_SETTING_CV1_GAIN, channel));
  }

  inline bool adc_filter_enabled(int channel) const {
    return get_value(channel_setting(IO_SETTING_CV1_FILTER, channel));
  }

  OutputVoltageScaling get_output_scaling(int channel) const {
    return static_cast<OutputVoltageScaling>(get_value(channel_setting(IO_SETTING_A_SCALING, channel)));
  }

  inline bool autotune_data_enabled(int channel) const {
    return get_value(channel_setting(IO_SETTING_A_TUNING, channel));
  }
};

// Structs to hold information about IO ports;
// Apps get queried and can fill this at runtime
struct OutputDesc {
  char label[9] = "";
  OutputMode mode = OUTPUT_MODE_RAW;

  void set(const char *l, OutputMode m) {
    strncpy(label, l, sizeof(label));
    mode = m;
  }
};

struct InputDesc {
  char label[9] = "";

  void set(const char *l) {
    strncpy(label, l, sizeof(label));
  }
};

struct IOConfig {
  InputDesc inputs[DAC_CHANNEL_LAST];
  OutputDesc outputs[DAC_CHANNEL_LAST];
};

struct IOFrame;

class IO {
public:
  static void ReadDigitalInputs(IOFrame *);
  static void ReadADC(IOFrame *, const IOSettings &);
  static void WriteDAC(IOFrame *, const IOSettings &);

  static int32_t pitch_rel_to_abs(int32_t pitch);
};

// Processing for apps works on a per-sample basis. An IOFrame encapuslates the
// IO data for a given frame, plus some helper functions to mimic the available
// functions in the direct drivers.
//
struct IOFrame {

  void Reset();

  struct {
    uint32_t rising_edges; // Rising edge detected since last frame
    uint32_t raised_mask;   // Last read state

    inline uint32_t triggered() const { return rising_edges; }

    template <DigitalInput input>
    inline uint32_t triggered() const { return rising_edges & DIGITAL_INPUT_MASK(input); }

    inline uint32_t triggered(DigitalInput input) const { return rising_edges & DIGITAL_INPUT_MASK(input); }

    template <DigitalInput input>
    inline bool raised() const { return raised_mask & DIGITAL_INPUT_MASK(input); }

    inline bool raised(DigitalInput input) const { return raised_mask & DIGITAL_INPUT_MASK(input); }

  } digital_inputs;

  struct {
    std::array<int32_t, ADC_CHANNEL_LAST> values;       // 10V = 2^12
    std::array<int32_t, ADC_CHANNEL_LAST> pitch_values; // 1V = 12 << 7 = 1536

    // Get CV value mapped for a given number of steps across the range
    // Round up/offset to move window and avoid "busy" toggling around 0
    // Works best of powers-of-two
    template <int32_t steps>
    constexpr int32_t ScaledValue(size_t channel) const {
      // TODO[PLD] size_t vs. ADC_CHANNEL
      // TODO[PLD] Ensure positive range allows for all values?
      // TODO[PLD] Rounding
      return (values[channel] * steps + (1 << 11) - 1) >> 12;
      //return (values[channel] * steps + (4096 / steps / 2 - 1)) >> 12;
    }

  } cv;

  struct {
    std::array<int32_t, DAC_CHANNEL_LAST> values;
    std::array<OutputMode, DAC_CHANNEL_LAST> modes;

    template <typename T>
    void set_pitch_values(const T& src)
    {
      std::copy(src, src + DAC_CHANNEL_LAST, std::begin(values));
      std::fill(std::begin(modes), std::end(modes), OUTPUT_MODE_PITCH);  
    }

    void set_pitch_value(size_t channel, int32_t value)
    {
      values[channel] = value;
      modes[channel] = OUTPUT_MODE_PITCH;
    }

    void set_gate_value(size_t channel, bool raised)
    {
      values[channel] = raised ? 1 : 0;
      modes[channel] = OUTPUT_MODE_GATE;
    }

    void set_unipolar_value(size_t channel, int32_t value)
    {
      values[channel] = value;
      modes[channel] = OUTPUT_MODE_UNI;
    }

    void set_raw_value(size_t channel, uint32_t value)
    {
      values[channel] = value;
      modes[channel] = OUTPUT_MODE_RAW;
    }

  } outputs;

};

} // namespace OC

#endif // OC_IO_H_
