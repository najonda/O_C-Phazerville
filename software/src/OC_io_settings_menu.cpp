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

void IOSettingsMenu::Edit(AppBase *app)
{
  io_settings_ = &app->mutable_io_settings();
  app->GetIOConfig(io_config_);

  cursor_.Init(IO_SETTING_CV1_GAIN, IO_SETTING_A_TUNING);
  cursor_.set_editing(false);
}

void IOSettingsMenu::Close()
{
  io_settings_ = nullptr;
}

void IOSettingsMenu::Draw() const
{
  using TitleBar = menu::QuadTitleBar;
  auto channel = selected_channel_;

  TitleBar::Draw(io_settings_->status_mask());
  for (int i = 0; i < 4; ++i) {
    TitleBar::SetColumn(i);
    graphics.print((char)('A' + i));
    graphics.movePrintPos(8, 0);
    graphics.printBitmap8(8, bitmap_output_mode_8 + io_config_.outputs[i].mode * 8);
  }
  TitleBar::Selected(channel);

  menu::SettingsList<menu::kScreenLines, 0, menu::kDefaultValueX> settings_list(cursor_);
  menu::SettingsListItem list_item;
  char label[16];

  while (settings_list.available()) {
    const IO_SETTING setting = static_cast<IO_SETTING>(settings_list.Next(list_item));
    const int value = io_settings_->get_value(IOSettings::channel_setting(setting, channel));
    const auto &attr = IOSettings::value_attributes(IOSettings::channel_setting(setting, channel));

    const auto &output_desc = io_config_.outputs[channel];

    switch (setting) {
    case IO_SETTING_A_SCALING: {
      snprintf(label, sizeof(label), "%s  %.10s",
               Strings::channel_id[channel],
               output_desc.label);
      //graphics.drawBitmap8(list_item.x + 2, list_item.y + 1, 8, bitmap_output_mode_8 + output_desc.mode * 8);
      if (output_desc.mode == OUTPUT_MODE_PITCH) {
        list_item.DrawCustomName(label, value, attr);
      } else {
        list_item.DrawCharName(label);
        list_item.DrawCustom();
      }
    }
    break;

    case IO_SETTING_CV1_GAIN: {
      const auto &input_desc = io_config_.inputs[channel];
      snprintf(label, sizeof(label),
          "%s %.10s",
          Strings::cv_input_names[channel],
          input_desc.label[0] ? input_desc.label : "????");
      list_item.DrawCustomName(label, value, attr);
    }
    break;

    case IO_SETTING_A_TUNING:
      if (OUTPUT_MODE_PITCH == output_desc.mode) {
        if (autotune_available()) {
          list_item.DrawDefault(value, attr);
        } else {
          // Perhaps also show if enabled, but not actually available?
          list_item.DrawCustomValue(attr, "----");
        }
      } else {
        list_item.DrawCustomValue(attr, "N/A");
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

UiMode IOSettingsMenu::DispatchEvent(const UI::Event &event)
{
  UiMode ui_mode = UI_MODE_MENU;
  if (UI::EVENT_BUTTON_PRESS == event.type) {
    switch (event.control) {
      case CONTROL_BUTTON_UP:
        Close(); // this is debatable
        ui_mode = UI_MODE_SCREENSAVER;
      break;
      case CONTROL_BUTTON_DOWN:
        Close();
      break;

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
  return ui_mode;
}

} // namespace OC
