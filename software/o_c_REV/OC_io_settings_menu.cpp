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

#include <Arduino.h>

#include "OC_config.h"
#include "OC_apps.h"
#include "OC_core.h"
#include "OC_cv_utils.h"
#include "OC_global_settings.h"
#include "OC_menus.h"
#include "OC_bitmaps.h"
#include "OC_strings.h"
#include "OC_ui.h"
#include "OC_io_settings_menu.h"

namespace OC {

void IOSettingsMenu::Init()
{
  io_settings_ = nullptr;
  selected_channel_ = 0;
}

void IOSettingsMenu::Edit(AppBase *app)
{
  io_settings_ = &app->mutable_io_settings();
  app->GetIOConfig(io_config_);

  cursor_.Init(IO_SETTING_CV1_GAIN, IO_SETTING_A_TUNING);
  cursor_.set_editing(false);
}

void IOSettingsMenu::Draw() const
{
  using TitleBar = menu::TitleBar<menu::kDefaultMenuStartX, 4, 2>;

  TitleBar::Draw();
  for (int i = 0; i < 4; ++i) {
    TitleBar::SetColumn(i);
    graphics.print((char)('A' + i));
    // TODO[PLD] Other status in titlebar?
/*
    graphics.print(
        CVUtils::kMultOne != io_settings_->get_value(IOSettings::channel_setting(IO_SETTING_CV1_GAIN, i))
          ? 'x' : ' ');
    graphics.print(
        io_settings_->get_value(IOSettings::channel_setting(IO_SETTING_CV1_FILTER, i))
          ? 'f' : ' ');

    graphics.print('<');
    graphics.print('*');
*/
  }
  TitleBar::Selected(selected_channel_);

  menu::SettingsList<menu::kScreenLines, 0, menu::kDefaultValueX> settings_list(cursor_);
  menu::SettingsListItem list_item;
  char label[16];

  while (settings_list.available()) {
    const IO_SETTING setting = static_cast<IO_SETTING>(settings_list.Next(list_item));
    const int value = io_settings_->get_value(IOSettings::channel_setting(setting, selected_channel_));
    const auto &attr = IOSettings::value_attributes(IOSettings::channel_setting(setting, selected_channel_));

    const auto &output_desc = io_config_.outputs[selected_channel_];

    switch (setting) {
    case IO_SETTING_A_SCALING: {
      graphics.drawBitmap8(list_item.x + 2, list_item.y + 1, 8, bitmap_output_mode_8 + output_desc.mode * 8);
      if (output_desc.mode == OUTPUT_MODE_PITCH) {
        list_item.DrawCustomName(output_desc.label, value, attr, 10);
      } else {
        list_item.DrawCharName(output_desc.label, 10);
        list_item.DrawCustom();
      }
    }
    break;

    case IO_SETTING_CV1_GAIN: {
      const auto &input_desc = io_config_.inputs[selected_channel_];
      snprintf(label, sizeof(label),
          "%s %.8s",
          Strings::cv_input_names[selected_channel_],
          input_desc.label[0] ? input_desc.label : "????");
      list_item.DrawCustomName(label, value, attr);
    }
    break;

    case IO_SETTING_A_TUNING:
      if (OUTPUT_MODE_PITCH == output_desc.mode) {
        list_item.DrawDefault(value, attr);
      } else {
        list_item.DrawCharName("DAC calibr.       N/A");
        list_item.DrawCustom();
      }
    break;

    case IO_SETTING_CV1_FILTER:
    default:
      list_item.DrawDefault(value, attr);
    break;
    }
  }
}

bool IOSettingsMenu::autotune_available() const
{
  return global_settings.autotune_calibration_data.channels[selected_channel_].is_valid();
}

void IOSettingsMenu::DispatchEvent(const UI::Event &event)
{
  if (UI::EVENT_BUTTON_PRESS == event.type) {
    switch (event.control) {
      case CONTROL_BUTTON_R:
        switch(cursor_.cursor_pos()) {
        case IO_SETTING_A_SCALING:
          if (OUTPUT_MODE_PITCH == io_config_.outputs[selected_channel_].mode)
            cursor_.toggle_editing();
        break;
        case IO_SETTING_A_TUNING:
          if (OUTPUT_MODE_PITCH == io_config_.outputs[selected_channel_].mode &&
              autotune_available())
            cursor_.toggle_editing();
        break;
        default:
          cursor_.toggle_editing();
        break;
        }
      break;
    }
  } else if (UI::EVENT_ENCODER == event.type) {
    switch(event.control) {
    case CONTROL_ENCODER_L: {
      int selected_channel = selected_channel_ + event.value;
      CONSTRAIN(selected_channel, 0, 3);
      selected_channel_ = selected_channel;
      cursor_.set_editing(false);
    }
    break;
    case CONTROL_ENCODER_R:
    if (cursor_.editing()) {
      io_settings_->change_value(
          IOSettings::channel_setting(
              static_cast<IO_SETTING>(cursor_.cursor_pos()),
              selected_channel_),
          event.value);
    } else {
      cursor_.Scroll(event.value);
    }
    break;
    }
  }
}

UiMode Ui::AppIOSettings(OC::AppBase *app)
{
  io_settings_menu_.Edit(app);

  bool exit_loop = false;
  while (!exit_loop) {
    GRAPHICS_BEGIN_FRAME(false);
    io_settings_menu_.Draw();
    GRAPHICS_END_FRAME();

    while (event_queue_.available()) {
      auto event = event_queue_.PullEvent();

      if (CONTROL_BUTTON_UP == event.control) {
        screensaver_ = true;
        return UI_MODE_SCREENSAVER;
      }
      if (CONTROL_BUTTON_DOWN == event.control) {
        exit_loop = true;
      }

      io_settings_menu_.DispatchEvent(event);
    }

    if (idle_time() > screensaver_timeout()) {
      screensaver_ = true;
      return UI_MODE_SCREENSAVER;
    }
  }

  return UI_MODE_MENU;
}

}
