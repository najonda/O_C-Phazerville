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
#include "OC_digital_inputs.h"
#include "OC_global_settings.h"
#include "util/util_misc.h"

#ifdef APPS_DEBUG
# define APPS_SERIAL_PRINTLN(msg, ...) serial_printf(msg "\n", ##__VA_ARGS__)
#else
# define APPS_SERIAL_PRINTLN(...)
#endif

namespace OC {

static AppBase * const available_apps[] = {
  &APP_ASR,
  &APP_H1200,
  &APP_AUTOMATONNETZ,
  &APP_QQ,
  &APP_DQ,
  &APP_POLYLFO,
  &APP_LORENZ,
  &APP_ENVGEN,
  &APP_A_SEQ,
  &APP_BBGEN,
  &APP_BYTEBEATGEN,
  &APP_CHORDS,
  &APP_REFS
};
static constexpr int NUM_AVAILABLE_APPS = ARRAY_SIZE(available_apps);

void AppBase::InitDefaults()
{
  io_settings_.InitDefaults();
  Init();
}

size_t AppBase::Save(util::StreamBufferWriter &stream_buffer) const
{
  size_t len = io_settings_.Save(stream_buffer);
  len += SaveAppData(stream_buffer);
  return len;
}

size_t AppBase::Restore(util::StreamBufferReader &stream_buffer)
{
  size_t len = io_settings_.Restore(stream_buffer);
  len += RestoreAppData(stream_buffer);
  return len;
}

// App settings are packed into a single blob of binary data; each app's chunk
// gets its own header with id and the length of the entire chunk. This makes
// this a bit more flexible during development.
// Chunks are aligned on 2-byte boundaries for arbitrary reasons (thankfully M4
// allows unaligned access...)
struct AppChunkHeader {
  uint16_t id;
  uint16_t length;
} __attribute__((packed));

struct AppData {
  static constexpr uint32_t FOURCC = FOURCC<'O','C','A',4>::value;
  static constexpr size_t kAppDataSize = EEPROM_APPDATA_BINARY_SIZE;

  char data[kAppDataSize];
  size_t used;
};

typedef PageStorage<EEPROMStorage, EEPROM_GLOBALSETTINGS_START, EEPROM_GLOBALSETTINGS_END, GlobalSettings> GlobalSettingsStorage;
typedef PageStorage<EEPROMStorage, EEPROM_APPDATA_START, EEPROM_APPDATA_END, AppData> AppDataStorage;

GlobalSettings global_settings;
GlobalSettingsStorage global_settings_storage;

AppData app_settings;
AppDataStorage app_data_storage;

AppSwitcher app_switcher;

static constexpr int DEFAULT_APP_INDEX = 0;
static const uint16_t DEFAULT_APP_ID = available_apps[DEFAULT_APP_INDEX]->id();

void save_global_settings() {
  APPS_SERIAL_PRINTLN("Save global settings");

  memcpy(global_settings.user_scales, OC::user_scales, sizeof(OC::user_scales));
  memcpy(global_settings.user_patterns, OC::user_patterns, sizeof(OC::user_patterns));
  memcpy(global_settings.user_chords, OC::user_chords, sizeof(OC::user_chords));
  
  global_settings_storage.Save(global_settings);
  APPS_SERIAL_PRINTLN("Saved global settings: page_index %d", global_settings_storage.page_index());
}

void save_app_data() {
  APPS_SERIAL_PRINTLN("Save app data... (%u bytes available)", OC::AppData::kAppDataSize);

  app_settings.used = 0;
  char *data = app_settings.data;
  char *data_end = data + OC::AppData::kAppDataSize;

  size_t start_app = random(NUM_AVAILABLE_APPS);
  for (size_t i = 0; i < NUM_AVAILABLE_APPS; ++i) {
    const auto &app = available_apps[(start_app + i) % NUM_AVAILABLE_APPS];
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
      APPS_SERIAL_PRINTLN("* %s (%02x) : Saved %u bytes... (%u)", app->name(), app->id(), result, storage_size);
      // TODO[PLD] Check result

      app_settings.used += chunk->length;
      data += chunk->length;
    }
  }
  APPS_SERIAL_PRINTLN("App settings used: %u/%u", app_settings.used, EEPROM_APPDATA_BINARY_SIZE);
  app_data_storage.Save(app_settings);
  APPS_SERIAL_PRINTLN("Saved app settings in page_index %d", app_data_storage.page_index());
}

void restore_app_data() {
  APPS_SERIAL_PRINTLN("Restoring app data from page_index %d, used=%u", app_data_storage.page_index(), app_settings.used);

  const char *data = app_settings.data;
  const char *data_end = data + app_settings.used;
  size_t restored_bytes = 0;

  while (data < data_end) {
    const AppChunkHeader *chunk = reinterpret_cast<const AppChunkHeader *>(data);
    if (data + chunk->length > data_end) {
      APPS_SERIAL_PRINTLN("App chunk length %u exceeds available space (%u)", chunk->length, data_end - data);
      break;
    }

    auto app = app_switcher.FindAppByID(chunk->id);
    if (!app) {
      APPS_SERIAL_PRINTLN("App %02x not found, ignoring chunk...", app->id());
      if (!chunk->length)
        break;
      data += chunk->length;
      continue;
    }
    size_t expected_length = app->storage_size() + sizeof(AppChunkHeader);
    if (expected_length & 0x1) ++expected_length;
    if (chunk->length != expected_length) {
      APPS_SERIAL_PRINTLN("* %s (%02x): chunk length %u != %u (storageSize=%u), skipping...", app->name(), chunk->id, chunk->length, expected_length, app->storage_size());
      data += chunk->length;
      continue;
    }

    util::StreamBufferReader stream_buffer{chunk + 1, chunk->length};
    auto result = app->Restore(stream_buffer);
    APPS_SERIAL_PRINTLN("* %s (%02x): Restored %u from %u (chunk length %u)...", app->name(), chunk->id, result, chunk->length - sizeof(AppChunkHeader), chunk->length);
    // TODO[PLD] Check result

    restored_bytes += chunk->length;
    data += chunk->length;
  }

  APPS_SERIAL_PRINTLN("App data restored: %u, expected %u", restored_bytes, app_settings.used);
}

void AppSwitcher::set_current_app(int index)
{
  current_app_ = available_apps[index];
  global_settings.current_app_id = current_app_->id();
}

AppBase *AppSwitcher::FindAppByID(uint16_t id) const {
  for (auto app : available_apps)
    if (app->id() == id) return app;
  return nullptr;
}

int AppSwitcher::IndexOfAppByID(uint16_t id) const {
  int i = 0;
  for (const auto app : available_apps) {
    if (app->id() == id) return i;
    ++i;
  }
  return i;
}

void AppSwitcher::Init(bool reset_settings) {

  current_app_ = available_apps[DEFAULT_APP_INDEX];

  for (auto &app : available_apps)
    app->InitDefaults();

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

    if (!app_data_storage.Load(app_settings)) {
      APPS_SERIAL_PRINTLN("Data not loaded, using defaults!");
    } else {
      restore_app_data();
    }
  }

  int current_app_index = app_switcher.IndexOfAppByID(global_settings.current_app_id);
  if (current_app_index < 0 || current_app_index >= NUM_AVAILABLE_APPS) {
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
    graphics.print(available_apps[current]->boring_name());
#else
    graphics.print(available_apps[current]->name());
#endif
    if (global_settings.current_app_id == available_apps[current]->id())
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
  cursor.Init(0, NUM_AVAILABLE_APPS - 1);
  cursor.Scroll(app_switcher.IndexOfAppByID(global_settings.current_app_id));

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
      save_global_settings();
      save_app_data();
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
