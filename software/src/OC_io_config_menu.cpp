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
#include "OC_menus.h"
#include "OC_bitmaps.h"
#include "OC_strings.h"
#include "OC_ui.h"
#include "OC_io_config_menu.h"

namespace OC {

void IOConfigMenu::Page::Init(const char *n, const uint8_t *i, int start, int end)
{
  name = n;
  icon = i;
  cursor.Init(start, end);
}

void IOConfigMenu::Init()
{
  current_page_ = INPUT_GAIN_PAGE;
  pages_[INPUT_GAIN_PAGE].Init("CV ", bitmap_bipolar_8x8, INPUT_SETTING_CV1_GAIN, INPUT_SETTING_CV4_GAIN);
  pages_[INPUT_FILTER_PAGE].Init("CV f", nullptr, INPUT_SETTING_CV1_FILTER, INPUT_SETTING_CV4_FILTER);
  pages_[OUTPUT_PAGE].Init("OUTs", nullptr, OUTPUT_SETTING_A_SCALING, OUTPUT_SETTING_LAST - 1);
  pages_[OUTPUT_TODO_PAGE].Init("????", nullptr, 0, 3);

  input_settings_ = nullptr;
  output_settings_ = nullptr;
}

void IOConfigMenu::Edit(App *app)
{
  input_settings_ = &app->input_settings;
  output_settings_ = &app->output_settings;
  
  app->GetIOConfig(io_config_);
}

void IOConfigMenu::Draw() const
{
  using TitleBar = menu::TitleBar<menu::kDefaultMenuStartX, 4, 2>;

  int column = 0;
  TitleBar::Draw();
  for (auto &page : pages_) {
    TitleBar::SetColumn(column++);
    graphics.print(page.name);
    if (page.icon) {
      graphics.movePrintPos(0, -1);
      graphics.printBitmap8(8, page.icon);
    }
  }
  TitleBar::Selected(current_page_);

  switch (current_page_) {
    case INPUT_GAIN_PAGE:
    case INPUT_FILTER_PAGE: DrawInputSettingsPage(); break;
    case OUTPUT_PAGE: DrawOutputPage(); break;
    case OUTPUT_TODO_PAGE: DrawTodoPage(); break;
    default: break;
  }
}

void IOConfigMenu::DrawInputSettingsPage() const
{
  menu::SettingsList<menu::kScreenLines, 0, menu::kDefaultValueX> settings_list(current_page().cursor);
  menu::SettingsListItem list_item;
  
  while (settings_list.available()) {
    const int setting = settings_list.Next(list_item);
    const int value = input_settings_->get_value(setting);
    const settings::value_attr &attr = InputSettings::value_attr(setting); 
    list_item.DrawDefault(value, attr);
  }
}

void IOConfigMenu::DrawOutputPage() const
{
  // NOTE Assuming there's only 4 lines here corresponding to the outputs
  menu::SettingsList<menu::kScreenLines, 0, menu::kDefaultValueX> settings_list(current_page().cursor);
  menu::SettingsListItem list_item;

  char label[16];
  while (settings_list.available()) {
    const int setting = settings_list.Next(list_item);
    auto channel = setting - OUTPUT_SETTING_A_SCALING;
    auto &desc = io_config_.outputs[channel];

    // TODO[PLD] Display if autotune is active for this channel

    snprintf(label, sizeof(label), "%s: %4s %s", Strings::channel_id[channel], desc.label, Strings::output_mode_strings[desc.mode]);

    if (desc.mode == OUTPUT_MODE_PITCH) {
      const int value = output_settings_->get_value(setting);
      const settings::value_attr &attr = OutputSettings::value_attr(setting); 
      list_item.DrawCustomName(label, value, attr);
    } else {
      list_item.DrawCharName(label);
      list_item.DrawCustom();
    }
  }
}

void IOConfigMenu::DrawTodoPage() const
{
}

void IOConfigMenu::DispatchEvent(const UI::Event &event)
{
  if (UI::EVENT_BUTTON_PRESS == event.type) {
    switch (event.control) {
      case CONTROL_BUTTON_R:
        if (OUTPUT_PAGE != current_page_ ||
            OUTPUT_MODE_PITCH == io_config_.outputs[current_page().cursor.cursor_pos()].mode)
          current_page().cursor.toggle_editing();
      break;
    }
  } else if (UI::EVENT_ENCODER == event.type) {
    switch(event.control) {
    case CONTROL_ENCODER_L: {
      int page = current_page_ + event.value;
      CONSTRAIN(page, 0, PAGE_LAST - 1);
      current_page_ = static_cast<PAGE>(page);
    }
    break;
    case CONTROL_ENCODER_R:
    if (current_page().cursor.editing()) {
      if (OUTPUT_PAGE == current_page_)
        output_settings_->change_value(current_page().cursor.cursor_pos(), event.value);
      else
        input_settings_->change_value(current_page().cursor.cursor_pos(), event.value);
    } else {
      current_page().cursor.Scroll(event.value);
    }
    break;
    }
  }
}

UiMode Ui::AppIOConfig(OC::App *app)
{
  io_config_menu_.Edit(app);

  bool exit_loop = false;
  while (!exit_loop) {
    GRAPHICS_BEGIN_FRAME(false);
    io_config_menu_.Draw();
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

      io_config_menu_.DispatchEvent(event);
    }

    if (idle_time() > screensaver_timeout()) {
      screensaver_ = true;
      return UI_MODE_SCREENSAVER;
    }
  }

  return UI_MODE_MENU;
}

}
