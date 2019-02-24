// Copyright (c) 2016 Patrick Dowling
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
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef OC_APP_H_
#define OC_APP_H_

#include "UI/ui_events.h"
#include "util/util_turing.h"
#include "util/util_misc.h"
#include "OC_io.h"

namespace OC {

enum AppEvent {
  APP_EVENT_SUSPEND,
  APP_EVENT_RESUME,
  APP_EVENT_SCREENSAVER_ON,
  APP_EVENT_SCREENSAVER_OFF
};

// This is a very poor-man's application "switching" framework, which has
// evolved out of the original (single-app) firmware.
//
// Using an interface class and virtual functions would probably provide a
// cleaner interface, but it's not worth bothering now.
//
// The additional call overhead of virtual functions can be avoided using a
// thunk table or other means, if it's in any way measurable. Only the ISR
// function is in a critical path anyway.
//
struct App {
  uint16_t id;
	const char *name;

  InputSettings input_settings;
  OutputSettings output_settings;

  void (*Init)(); // one-time init
  size_t (*storageSize)(); // binary size of storage requirements
  size_t (*Save)(void *);
  size_t (*Restore)(const void *);

  void (*HandleAppEvent)(AppEvent); // Generic event handler

  void (*loop)(); // main loop function
  void (*DrawMenu)();
  void (*DrawScreensaver)();

  void (*HandleButtonEvent)(const UI::Event &);
  void (*HandleEncoderEvent)(const UI::Event &);

  void (*Process)(IOFrame *ioframe);

  void (*GetIOConfig)(IOConfig &config);
};

namespace apps {

  extern const App *current_app;

  void Init(bool reset_settings);

  inline void Process(IOFrame *ioframe) __attribute__((always_inline));
  inline void Process(IOFrame *ioframe) {
    if (current_app) {
      IO::ReadDigitalInputs(ioframe);
      IO::ReadADC(ioframe, current_app->input_settings);
      current_app->Process(ioframe);
      IO::WriteDAC(ioframe, current_app->output_settings);
    }
  }

  const App *find(uint16_t id);
  int index_of(uint16_t id);

}; // namespace apps

void draw_save_message(uint8_t c);
void save_app_data();

}; // namespace OC

#endif // OC_APP_H_
