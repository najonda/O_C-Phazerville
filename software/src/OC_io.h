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
#include <stdint.h>
#include "util/util_math.h"

namespace OC {

// The dependency here is weird.
enum DigitalInput {
  DIGITAL_INPUT_1,
  DIGITAL_INPUT_2,
  DIGITAL_INPUT_3,
  DIGITAL_INPUT_4,
  DIGITAL_INPUT_LAST
};

#define DIGITAL_INPUT_MASK(x) (0x1 << (x))

// Output value types
enum OutputMode {
  OUTPUT_MODE_PITCH,
  OUTPUT_MODE_GATE,
  OUTPUT_MODE_UNI,
  OUTPUT_MODE_RAW,
};

static constexpr size_t kMaxOutputChannels = 4;
static constexpr size_t kMaxInputChannels  = 4;

struct IOConfig {
  std::array<OutputMode, kMaxOutputChannels> output_modes;
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
    std::array<int32_t, kMaxInputChannels> values;       // 10V = 2^12
    std::array<int32_t, kMaxInputChannels> pitch_values; // 1V = 12 << 7 = 1536

    // Get CV value mapped for a given number of steps across the range
    // Round up/offset to move window and avoid "busy" toggling around 0
    // Works best of powers-of-two
    template <int32_t steps>
    constexpr int32_t Value(size_t channel) const {
      // TODO[PLD] Ensure positive range allows for all values?
      // TODO[PLD] Rounding
      return (values[channel] * steps + (4096 / steps / 2 - 1)) >> 12;
    }

  } cv;

  struct {
    std::array<int32_t, kMaxOutputChannels> values;
    std::array<OutputMode, kMaxOutputChannels> modes;

    template <typename T>
    void set_pitch_values(const T& src)
    {
      std::copy(src, src + kMaxOutputChannels, std::begin(values));
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
