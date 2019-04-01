// Copyright (c) 2016-2019 Patrick Dowling
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
#include "util/util_misc.h"
#include "OC_io.h"

namespace OC {

enum AppEvent {
  APP_EVENT_SUSPEND,
  APP_EVENT_RESUME,
  APP_EVENT_SCREENSAVER_ON,
  APP_EVENT_SCREENSAVER_OFF
};

// The original "app" interface was built around structs filled with function
// pointers, i.e. without any form of "this" pointer. Mainly because that's
// how it evolved out of the original (single-app) firmware and protoapps.
//
// Adding the IO settings to each app, and having them be able to access it
// from within meant either:
// a) adding it as a parameter to all functions, or
// b) biting the refactoring bullet.
//
// Obviously, the choice was c).
//
// Using virtual functions make it (slightly) less trivial to instantiate an
// app (i.e. it can't just be an array of app) but that's the price to pay...
//
// It's possible there might be optimized ways of calling the virtual functions
// but only the ISR (Process) is in the critical path.
//
class AppBase {
public:
  virtual ~AppBase() { }
  
  inline uint16_t id() const { return id_; }
  inline const char *name() const { return name_; }

  IOSettings &mutable_io_settings() { return io_settings_; }
  const IOSettings &io_settings() const { return io_settings_; }

  void InitDefaults();
  size_t storage_size() const { return IOSettings::storageSize() + appdata_storage_size(); }
  size_t Save(void *) const;
  size_t Restore(const void *);

  // Main implementation interface for derived classes
  virtual void Init() = 0;
  virtual size_t appdata_storage_size() const = 0;
  virtual size_t SaveAppData(void *) const = 0;
  virtual size_t RestoreAppData(const void *) = 0;
  virtual void HandleAppEvent(AppEvent) = 0;
  virtual void Loop() = 0;
  virtual void DrawMenu() const = 0;
  virtual void DrawScreensaver() const = 0;
  virtual void HandleButtonEvent(const UI::Event &) = 0;
  virtual void HandleEncoderEvent(const UI::Event &) = 0;
  virtual void Process(IOFrame *ioframe) = 0;
  virtual void GetIOConfig(IOConfig &) const = 0;
  virtual void DrawDebugInfo() const = 0;

private:
  const uint16_t id_;
  const char * const name_;
  const char * const boring_name_;

protected:
  IOSettings io_settings_;

  AppBase(uint16_t appid, const char *const appname, const char *const boring_name)
  : id_(appid), name_(appname), boring_name_(boring_name) { }

  DISALLOW_COPY_AND_ASSIGN(AppBase);
};

template <typename T, typename Traits> class AppBaseImpl : public AppBase {
protected:
  static constexpr uint16_t kAppId = Traits::id;
  static constexpr const char *const kAppName = Traits::app_name;
  static constexpr const char *const kBoringAppName = Traits::boring_app_name;

  AppBaseImpl() : AppBase(kAppId, kAppName, kBoringAppName) { }
};

#define OC_APP_TRAITS(clazz, app_id, name, boring_name) \
struct MACRO_CONCAT(clazz, Traits) { \
  static constexpr uint16_t id = app_id; \
  static constexpr const char *const app_name = name; \
  static constexpr const char *const boring_app_name = boring_name; \
}

#define OC_APP_CLASS(clazz) \
clazz : public AppBaseImpl<clazz, MACRO_CONCAT(clazz, Traits)>

#define OC_APP_INTERFACE_DECLARE(clazz) \
public: \
  clazz() : AppBaseImpl<clazz, MACRO_CONCAT(clazz, Traits)>() { } \
  virtual void Init() final; \
  virtual size_t appdata_storage_size() const final; \
  virtual size_t SaveAppData(void *) const final; \
  virtual size_t RestoreAppData(const void *) final; \
  virtual void HandleAppEvent(AppEvent) final; \
  virtual void Loop() final; \
  virtual void DrawMenu() const final; \
  virtual void DrawScreensaver() const final; \
  virtual void HandleButtonEvent(const UI::Event &) final; \
  virtual void HandleEncoderEvent(const UI::Event &) final; \
  virtual void Process(IOFrame *ioframe) final; \
  virtual void GetIOConfig(IOConfig &) const final; \
  virtual void DrawDebugInfo() const final

// Minimal app-switching helper/manager.
// Use with care.
class AppSwitcher {
public:
  AppSwitcher() { }
  ~AppSwitcher() { }

  void Init(bool reset_settings);

  inline AppBase *current_app() const { return current_app_; }

  inline void Process(IOFrame *ioframe) __attribute__((always_inline)) {
    if (current_app_) {
      IO::ReadDigitalInputs(ioframe);
      IO::ReadADC(ioframe, current_app_->io_settings());
      current_app_->Process(ioframe);
      IO::WriteDAC(ioframe, current_app_->io_settings());
    }
  }

  AppBase *FindAppByID(uint16_t id) const;
  int IndexOfAppByID(uint16_t id) const;
  void set_current_app(int index);

private:
  AppBase *current_app_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AppSwitcher);
};

extern AppSwitcher app_switcher;

}; // namespace OC

#endif // OC_APP_H_
