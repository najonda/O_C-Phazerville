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

#include "OC_apps.h"
#include "OC_storage.h"
#include "OC_app_switcher.h"
#include "OC_global_settings.h"
#include "util/util_misc.h"

#ifdef APPS_DEBUG
# define APPS_SERIAL_PRINTLN(msg, ...) serial_printf(msg "\n", ##__VA_ARGS__)
#else
# define APPS_SERIAL_PRINTLN(...)
#endif

namespace OC {

GlobalSettings global_settings;
AppSwitcher app_switcher;

static AppData app_data;
static GlobalSettingsStorage global_settings_storage;
static AppDataStorage app_data_storage;

static AppContainer<void // this space intentionally left blank
  , AppASR
  , AppH1200
  , AppAutomatonnetz
  , AppQuadQuantizer
  , AppDualQuantizer
  , AppPolyLfo
  , AppLorenzGenerator
  , AppQuadEnvelopeGenerator
  , AppDualSequencer
  , AppQuadBouncingBalls
  , AppQuadByteBeats
  , AppChordQuantizer
  , AppReferences
> app_container;

static_assert(decltype(app_container)::TotalAppDataStorageSize() < AppData::kAppDataSize,
              "Apps use too much EEPROM space!");

static constexpr int DEFAULT_APP_INDEX = 0;
static constexpr uint16_t DEFAULT_APP_ID = decltype(app_container)::GetAppIDAtIndex<DEFAULT_APP_INDEX>();


void AppBase::InitDefaults()
{
  io_settings_.InitDefaults();
  Init();
}

size_t AppBase::Save(util::StreamBufferWriter &stream_buffer) const
{
  io_settings_.Save(stream_buffer);
  SaveAppData(stream_buffer);
  return stream_buffer.written();
}

size_t AppBase::Restore(util::StreamBufferReader &stream_buffer)
{
  io_settings_.Restore(stream_buffer);
  RestoreAppData(stream_buffer);
  return stream_buffer.read();
}


static void SaveGlobalSettings() {
  APPS_SERIAL_PRINTLN("Save global settings");

  memcpy(global_settings.user_scales, OC::user_scales, sizeof(OC::user_scales));
  memcpy(global_settings.user_patterns, OC::user_patterns, sizeof(OC::user_patterns));
  memcpy(global_settings.user_chords, OC::user_chords, sizeof(OC::user_chords));
  
  global_settings_storage.Save(global_settings);
  APPS_SERIAL_PRINTLN("Saved global settings: page_index %d", global_settings_storage.page_index());
}

static void SaveAppData() {
  APPS_SERIAL_PRINTLN("Save app data... (%u bytes available)", OC::AppData::kAppDataSize);

  app_data.used = 0;
  uint8_t *data = app_data.data;
  uint8_t *data_end = data + OC::AppData::kAppDataSize;

  size_t start_app = random(app_container.num_apps());
  for (size_t i = 0; i < app_container.num_apps(); ++i) {
    const auto app = app_container[(start_app + i) % app_container.num_apps()];
    size_t storage_size = app->storage_size() + sizeof(AppChunkHeader);
    if (storage_size & 1) ++storage_size; // Align chunks on 2-byte boundaries
    if (storage_size > sizeof(AppChunkHeader)) {
      if (data + storage_size > data_end) {
        APPS_SERIAL_PRINTLN("%s: ERROR: %u BYTES NEEDED, %u BYTES AVAILABLE OF %u BYTES TOTAL", app->name(), storage_size, data_end - data, AppData::kAppDataSize);
        continue;
      }

      AppChunkHeader *chunk = reinterpret_cast<AppChunkHeader *>(data);
      chunk->id = app->id();
      chunk->length = storage_size;

      util::StreamBufferWriter stream_buffer{chunk + 1, chunk->length};
      auto result = app->Save(stream_buffer);
      if (stream_buffer.overflow()) {
        APPS_SERIAL_PRINTLN("* %s (%02x) : Save overflowed, result=%u, skipping app...", 
                            app->name(), app->id(), result);
      } else {
        APPS_SERIAL_PRINTLN("* %s (%02x) : Saved %u bytes... (%u)",
                            app->name(), app->id(), result, storage_size);
        app_data.used += chunk->length;
        data += chunk->length;
      }
      (void)result;
    }
  }
  APPS_SERIAL_PRINTLN("App settings used: %u/%u", app_data.used, EEPROM_APPDATA_BINARY_SIZE);
  app_data_storage.Save(app_data);
  APPS_SERIAL_PRINTLN("Saved app settings in page_index %d", app_data_storage.page_index());
}

static void RestoreAppData() {
  APPS_SERIAL_PRINTLN("Restoring app data from page_index %d, used=%u", app_data_storage.page_index(), app_data.used);

  const uint8_t *data = app_data.data;
  const uint8_t *data_end = data + app_data.used;
  size_t restored_bytes = 0;

  while (data < data_end) {
    const AppChunkHeader *chunk = reinterpret_cast<const AppChunkHeader *>(data);
    if (data + chunk->length > data_end) {
      APPS_SERIAL_PRINTLN("App chunk length %u exceeds available space (%u)", chunk->length, data_end - data);
      break;
    }
    if (!chunk->id || !chunk->length) {
      APPS_SERIAL_PRINTLN("Invalid app chunk id=%02x, length=%d, stopping restore", chunk->id, chunk->length);
      break;
    }

    auto app = app_container.FindAppByID(chunk->id);
    if (!app) {
      APPS_SERIAL_PRINTLN("App %02x not found, ignoring chunk...", chunk->id);
      data += chunk->length;
      continue;
    }
    size_t expected_length = app->storage_size() + sizeof(AppChunkHeader);
    if (expected_length & 0x1) ++expected_length;
    if (chunk->length != expected_length) {
      APPS_SERIAL_PRINTLN("* %s (%02x): chunk length %u != %u (storage_size=%u), skipping...", app->name(), chunk->id, chunk->length, expected_length, app->storage_size());
      data += chunk->length;
      continue;
    }

    util::StreamBufferReader stream_buffer{chunk + 1, chunk->length};
    auto result = app->Restore(stream_buffer);
    if (stream_buffer.underflow()) {
      APPS_SERIAL_PRINTLN("* %s (%02x): Restore underflow, result=%u, re-init",
                          app->name(), chunk->id, result);
      app->InitDefaults();
    } else {
      APPS_SERIAL_PRINTLN("* %s (%02x): Restored %u from %u (chunk length %u)...",
                          app->name(), chunk->id, result, chunk->length - sizeof(AppChunkHeader), chunk->length);
      restored_bytes += chunk->length;
    }
    (void)result;

    data += chunk->length;
  }

  APPS_SERIAL_PRINTLN("App data restored: %u, expected %u", restored_bytes, app_data.used);
}

void AppSwitcher::set_current_app(size_t index)
{
  current_app_ = app_container[index];
  global_settings.current_app_id = current_app_->id();
}

void AppSwitcher::Init(bool reset_settings) {

  APPS_SERIAL_PRINTLN("Init");
  app_container.for_each([](AppBase *app) {
    APPS_SERIAL_PRINTLN("> %s", app->name());
    app->InitDefaults();
  });

  current_app_ = app_container[DEFAULT_APP_INDEX];

  global_settings.Init();
  global_settings.encoders_enable_acceleration = OC_ENCODERS_ENABLE_ACCELERATION_DEFAULT;
  global_settings.reserved0 = false;
  global_settings.reserved1 = false;
  global_settings.reserved2 = 0U;
  global_settings.current_app_id = DEFAULT_APP_ID;

  if (reset_settings) {
    if (ui.ConfirmReset()) {
      APPS_SERIAL_PRINTLN("Erase EEPROM ...");
      EEPtr d = EEPROM_GLOBALSETTINGS_START;
      size_t len = EEPROMStorage::LENGTH - EEPROM_GLOBALSETTINGS_START;
      while (len--)
        *d++ = 0;
      APPS_SERIAL_PRINTLN("...done");
      APPS_SERIAL_PRINTLN("Skip settings, using defaults...");
      global_settings_storage.Init();
      app_data_storage.Init();
    } else {
      reset_settings = false;
    }
  }

  if (!reset_settings) {
    APPS_SERIAL_PRINTLN("Load global settings: size: %u, PAGESIZE=%u, PAGES=%u, LENGTH=%u",
                  sizeof(GlobalSettings),
                  GlobalSettingsStorage::PAGESIZE,
                  GlobalSettingsStorage::PAGES,
                  GlobalSettingsStorage::LENGTH);

    if (!global_settings_storage.Load(global_settings)) {
      APPS_SERIAL_PRINTLN("Settings invalid, using defaults!");
    } else {
      APPS_SERIAL_PRINTLN("Loaded settings from page_index %d, current_app_id is %02x",
                    global_settings_storage.page_index(),global_settings.current_app_id);
      memcpy(user_scales, global_settings.user_scales, sizeof(user_scales));
      memcpy(user_patterns, global_settings.user_patterns, sizeof(user_patterns));
      memcpy(user_chords, global_settings.user_chords, sizeof(user_chords));
    }

    APPS_SERIAL_PRINTLN("Load app data: size is %u, PAGESIZE=%u, PAGES=%u, LENGTH=%u",
                  sizeof(AppData),
                  AppDataStorage::PAGESIZE,
                  AppDataStorage::PAGES,
                  AppDataStorage::LENGTH);

    if (!app_data_storage.Load(app_data)) {
      APPS_SERIAL_PRINTLN("Data not loaded, using defaults!");
    } else {
      RestoreAppData();
    }
  }

  auto current_app_index = app_container.IndexOfAppByID(global_settings.current_app_id);
  if (current_app_index >= app_container.num_apps()) {
    APPS_SERIAL_PRINTLN("App id %02x not found, using default!", global_settings.current_app_id);
    global_settings.current_app_id = DEFAULT_APP_ID;
    current_app_index = DEFAULT_APP_INDEX;
  }

  APPS_SERIAL_PRINTLN("Encoder acceleration: %s", global_settings.encoders_enable_acceleration ? "enabled" : "disabled");
  ui.encoders_enable_acceleration(global_settings.encoders_enable_acceleration);

  set_current_app(current_app_index);
  current_app_->HandleAppEvent(APP_EVENT_RESUME);

  delay(100);
}

void draw_app_menu(const menu::ScreenCursor<5> &cursor) {
  GRAPHICS_BEGIN_FRAME(true);

  menu::SettingsListItem item;
  item.x = menu::kIndentDx;
  item.y = (64 - (5 * menu::kMenuLineH)) / 2;

  for (int current = cursor.first_visible();
       current <= cursor.last_visible();
       ++current, item.y += menu::kMenuLineH) {

    item.selected = current == cursor.cursor_pos();
    item.SetPrintPos();
    graphics.movePrintPos(weegfx::Graphics::kFixedFontW, 0);
#ifdef BORING_APP_NAMES
    graphics.print(app_container[current]->boring_name());
#else
    graphics.print(app_container[current]->name());
#endif
    if (global_settings.current_app_id == app_container[current]->id())
       graphics.drawBitmap8(item.x + 2, item.y + 1, 4, bitmap_indicator_4x8);
    item.DrawCustom();
  }

  GRAPHICS_END_FRAME();
}

void draw_save_message(uint8_t c) {
  GRAPHICS_BEGIN_FRAME(true);
  uint8_t _size = c % 120;
  graphics.drawRect(63 - (_size >> 1), 31 - (_size >> 2), _size, _size >> 1);  
  GRAPHICS_END_FRAME();
}

void Ui::AppSettings() {

  SetButtonIgnoreMask();

  app_switcher.current_app()->HandleAppEvent(APP_EVENT_SUSPEND);

  menu::ScreenCursor<5> cursor;
  cursor.Init(0, app_container.num_apps() - 1);
  cursor.Scroll(app_container.IndexOfAppByID(global_settings.current_app_id));

  bool change_app = false;
  bool save = false;
  while (!change_app && idle_time() < APP_SELECTION_TIMEOUT_MS) {

    while (event_queue_.available()) {
      UI::Event event = event_queue_.PullEvent();
      if (IgnoreEvent(event))
        continue;

      if (UI::EVENT_ENCODER == event.type && CONTROL_ENCODER_R == event.control) {
        cursor.Scroll(event.value);
      } else if (CONTROL_BUTTON_R == event.control) {
        save = event.type == UI::EVENT_BUTTON_LONG_PRESS;
        change_app = true;
      } else if (CONTROL_BUTTON_L == event.control) {
        ui.DebugStats();
      } else if (CONTROL_BUTTON_UP == event.control) {
        bool enabled = !global_settings.encoders_enable_acceleration;
        APPS_SERIAL_PRINTLN("Encoder acceleration: %s", enabled ? "enabled" : "disabled");
        ui.encoders_enable_acceleration(enabled);
        global_settings.encoders_enable_acceleration = enabled;
      }
    }

    draw_app_menu(cursor);
  }

  event_queue_.Flush();
  event_queue_.Poke();

  CORE::app_isr_enabled = false;
  delay(1);

  if (change_app) {
    app_switcher.set_current_app(cursor.cursor_pos());
    FreqMeasure.end();
    OC::DigitalInputs::reInit();
    if (save) {
      SaveGlobalSettings();
      SaveAppData();
      // draw message:
      int cnt = 0;
      while(idle_time() < SETTINGS_SAVE_TIMEOUT_MS)
        draw_save_message((cnt++) >> 4);
    }
  }

  OC::ui.encoders_enable_acceleration(global_settings.encoders_enable_acceleration);

  // Restore state
  app_switcher.current_app()->HandleAppEvent(APP_EVENT_RESUME);
  CORE::app_isr_enabled = true;
}

bool Ui::ConfirmReset() {

  SetButtonIgnoreMask();

  bool done = false;
  bool confirm = false;

  do {
    while (event_queue_.available()) {
      UI::Event event = event_queue_.PullEvent();
      if (IgnoreEvent(event))
        continue;
      if (CONTROL_BUTTON_R == event.control) {
        confirm = true;
        done = true;
      } else if (CONTROL_BUTTON_L == event.control) {
        confirm = false;
        done = true;
      }
    }

    GRAPHICS_BEGIN_FRAME(true);
    weegfx::coord_t y = menu::CalcLineY(0);
    graphics.setPrintPos(menu::kIndentDx, y);
    graphics.print("[L] EXIT");
    y += menu::kMenuLineH;

    graphics.setPrintPos(menu::kIndentDx, y);
    graphics.print("[R] RESET SETTINGS" );
    y += menu::kMenuLineH;
    graphics.setPrintPos(menu::kIndentDx, y);
    graphics.print("    AND ERASE EEPROM");
    GRAPHICS_END_FRAME();

  } while (!done);

  return confirm;
}

}; // namespace OC
