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
#ifndef OC_APP_SWITCHER_H_
#define OC_APP_SWITCHER_H_

#include <array>
#include <tuple>
#include "OC_apps.h"
#include "util/util_templates.h"

namespace OC { 

// Minimal app-switching helper/manager/general dogsbody.
// Use with care.
class AppSwitcher {
public:
  AppSwitcher() { }
  ~AppSwitcher() { }

  void Init(bool reset_settings);

  void set_current_app(size_t index);
  inline AppBase *current_app() const { return current_app_; }

  inline void Process(IOFrame *ioframe) __attribute__((always_inline)) {
    if (current_app_) {
      IO::ReadDigitalInputs(ioframe);
      IO::ReadADC(ioframe, current_app_->io_settings());
      current_app_->Process(ioframe);
      IO::WriteDAC(ioframe, current_app_->io_settings());
    }
  }

private:
  AppBase *current_app_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AppSwitcher);
};

extern AppSwitcher app_switcher;

// Helpers

template <typename Tuple, size_t... I>
std::array<AppBase *, std::tuple_size<Tuple>::value> tuple_to_array(Tuple &tuple, util::index_sequence<I...>) {
    return std::array<AppBase *, std::tuple_size<Tuple>::value>{ &std::get<I>(tuple)... };
}

template <typename Tuple>
std::array<AppBase *, std::tuple_size<Tuple>::value> tuple_to_array(Tuple &tuple) {
    return tuple_to_array(tuple, typename util::make_index_sequence<std::tuple_size<Tuple>::value>::type());
}

template <typename T, typename... Ts>
class AppContainer {
private:
  using tuple_type = std::tuple<Ts...>;
  tuple_type instances;

public:

  template <size_t N>
  static constexpr uint16_t GetAppIDAtIndex() {
    return std::tuple_element<N, tuple_type>::type::kAppId;
  }

  static constexpr size_t kNumApps = sizeof...(Ts);

  constexpr size_t num_apps() const {
    return kNumApps;
  }

  inline AppBase * operator [](size_t i) { return pointers[i]; }

  AppBase *FindAppByID(uint16_t id) const {
    auto result =
      std::find_if(std::begin(pointers), std::end(pointers),
                   [&id](AppBase *app) { return app->id() == id; });
    return result != std::end(pointers) ? *result : nullptr;
  }

  size_t IndexOfAppByID(uint16_t id) const {
    auto result =
      std::find_if(std::begin(pointers), std::end(pointers),
                   [&id](AppBase *app) { return app->id() == id; });
    return result != std::end(pointers) ? std::distance(std::begin(pointers), result) : num_apps();
  }

  const std::array<AppBase *, kNumApps> pointers = tuple_to_array(instances);
};

} // OC

#endif // OC_APP_SWITCHER_H_
