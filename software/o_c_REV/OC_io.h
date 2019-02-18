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

#include "util/util_math.h"

namespace OC {

enum DigitalInput {
  DIGITAL_INPUT_1,
  DIGITAL_INPUT_2,
  DIGITAL_INPUT_3,
  DIGITAL_INPUT_4,
  DIGITAL_INPUT_LAST
};

#define DIGITAL_INPUT_MASK(x) (0x1 << (x))

struct IOFrame {

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

};

}; // namespace OC

#endif // OC_IO_H_
