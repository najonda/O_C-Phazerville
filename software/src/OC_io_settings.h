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
#ifndef OC_IO_SETTINGS_H_
#define OC_IO_SETTINGS_H_

#include "OC_DAC.h"
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

  unsigned status_mask(int channel) const {
    return
      (input_gain(channel) ? 0x1 : 0x0) |
      (adc_filter_enabled(channel) ? 0x2 : 0x0) |
      (get_output_scaling(channel) ? 0x4 : 0x0) |
      (autotune_data_enabled(channel) ? 0x8 : 0x0);
  }
};

} // OC

#endif // OC_IO_SETTINGS_H_
