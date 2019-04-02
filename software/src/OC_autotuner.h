#ifndef OC_AUTOTUNER_H
#define OC_AUTOTUNER_H

#include "OC_options.h"
#include "OC_visualfx.h"
#include "OC_DAC.h"
#include "OC_global_settings.h"
#include "OC_menus.h"
#include "OC_strings.h"
#include "OC_ui.h"

namespace OC {

enum AUTO_MENU_ITEMS {
  DATA_SELECT,
  AUTOTUNE,
  AUTORUN,
  AUTO_MENU_ITEMS_LAST
};

enum AT_STATUS {
   AT_OFF,
   AT_READY,
   AT_WAIT,
   AT_RUN,
   AT_ERROR,
   AT_DONE,
   AT_LAST,
};

enum AUTO_CALIBRATION_STEP {
  DAC_VOLT_0_ARM,
  DAC_VOLT_0_BASELINE,
  DAC_VOLT_TARGET_FREQUENCIES,
  DAC_VOLT_WAIT,
  DAC_VOLT_3m, 
  DAC_VOLT_2m, 
  DAC_VOLT_1m, 
  DAC_VOLT_0, 
  DAC_VOLT_1, 
  DAC_VOLT_2, 
  DAC_VOLT_3, 
  DAC_VOLT_4, 
  DAC_VOLT_5, 
  DAC_VOLT_6,
#ifdef VOR
  DAC_VOLT_7,
#endif
  AUTO_CALIBRATION_STEP_LAST
};

extern const char* const AT_steps[];

enum AUTOTUNER_SETTINGS {
  AT_SETTING_RESET,
  AT_SETTING_START,
  AT_SETTING_END,
  AT_SETTING_ACTION,
  AUTOTUNER_SETTINGS_LAST
};

enum AUTOTUNER_RESET_ACTION {
  AUTOTUNER_RESET, AUTOTUNER_USE, AUTOTUNER_RESET_ACTION_LAST
};

enum AUTOTUNER_STATUS_ACTION {
  AUTOTUNER_ARM, AUTOTUNER_RUN, AUTOTUNER_STOP, AUTOTUNER_OK, AUTOTUNER_STATUS_ACTION_LAST
};

class AutotunerSettings : public settings::SettingsBase<AutotunerSettings, AUTOTUNER_SETTINGS_LAST> {
public:
  int start_offset() const { return get_value(AT_SETTING_START); }
  int end_offset() const { return get_value(AT_SETTING_END); }

  AUTOTUNER_STATUS_ACTION get_status_action() const {
    return static_cast<AUTOTUNER_STATUS_ACTION>(get_value(AT_SETTING_ACTION));
  }

  AUTOTUNER_RESET_ACTION get_reset_action() const {
    return static_cast<AUTOTUNER_RESET_ACTION>(get_value(AT_SETTING_RESET));
  }
};

// This is a slight refactoring of the original implementation.
// It tries to treat the Owner as the model, and implements both the view and
// controller parts, i.e. doesn't actually have many smarts.
//
// Ideally (for some value of ideal) the autotuning state might be extracted
// from the channel, and this would interface with that without the templatery.
//
template <typename Owner>
class Autotuner {
public:

  static constexpr uint32_t kAnimationTicks = OC_CORE_ISR_FREQ / 2; 

  void Init() {
    owner_ = nullptr;
    channel_ = DAC_CHANNEL_D;
    calibration_data_ = nullptr;

    settings_.InitDefaults();
    cursor_.Init(AT_SETTING_RESET, AT_SETTING_ACTION);

    memset(status_action_label_, 0, sizeof(status_action_label_));
    ticks_ = 0;
    animation_counter_ = 0;
  }

  inline bool active() const {
    return nullptr != owner_;
  }

  void Open(Owner *owner);
  void Close();
  void Update();
  void Tick();
  void Draw() const;
  void HandleButtonEvent(const UI::Event &event);
  void HandleEncoderEvent(const UI::Event &event);

private:

  Owner *owner_;

  DAC_CHANNEL channel_;
  const DAC::AutotuneChannelCalibrationData *calibration_data_;

  AutotunerSettings settings_;
  menu::ScreenCursor<menu::kScreenLines> cursor_;

  char status_action_label_[16];
  uint32_t ticks_;
  uint_fast8_t animation_counter_;

  void DoReset(AUTOTUNER_RESET_ACTION reset_action);
  void DoAction(AUTOTUNER_STATUS_ACTION status_action);

  void handleButtonLeft(const UI::Event &event);
  void handleButtonRight(const UI::Event &event);
  void handleButtonUp(const UI::Event &event);
  void handleButtonDown(const UI::Event &event);
};
  
  template <typename Owner>
  void Autotuner<Owner>::Open(Owner *owner) {
    owner_ = owner;
    channel_ = owner_->get_channel();
    calibration_data_ = &global_settings.autotune_calibration_data.channels[channel_];
    cursor_.Init(AT_SETTING_RESET, AT_SETTING_ACTION);
    settings_.InitDefaults();
    memset(status_action_label_, 0, sizeof(status_action_label_));

    Update();
  }

  template <typename Owner>
  void Autotuner<Owner>::Close() {
    ui.SetButtonIgnoreMask();
    if (owner_) {
      owner_->autotuner_reset();
      owner_ = nullptr;
    }
  }

  template <typename Owner>
  void Autotuner<Owner>::DoReset(AUTOTUNER_RESET_ACTION reset_action) {
  }

  template <typename Owner>
  void Autotuner<Owner>::DoAction(AUTOTUNER_STATUS_ACTION status_action) {
    switch(status_action) {
    case AUTOTUNER_ARM: owner_->autotuner_arm(); break;
    case AUTOTUNER_RUN: owner_->autotuner_run(); break;
    case AUTOTUNER_STOP:
    case AUTOTUNER_OK: owner_->autotuner_reset(); break;
    default: break;
    }
  }

  template <typename Owner>
  void Autotuner<Owner>::Update() {
    static constexpr const char *ellipses[4] = { "   ", ".  ", ".. ", "..." };
    if (owner_->autotuner_active()) {
      auto error = owner_->autotuner_error();
      if (error) {
        if (animation_counter_ > 4)
          snprintf(status_action_label_, sizeof(status_action_label_), "Autotune fail");
        else
          snprintf(status_action_label_, sizeof(status_action_label_), "%s %d", error, owner_->get_octave_cnt());
        settings_.apply_value(AT_SETTING_ACTION, AUTOTUNER_OK);
      } else if (owner_->autotuner_completed()){
        snprintf(status_action_label_, sizeof(status_action_label_), "Autotune done");
        settings_.apply_value(AT_SETTING_ACTION, AUTOTUNER_OK);
      } else {
        auto step = owner_->autotuner_step();
        switch(step) {
        case DAC_VOLT_0_ARM:
          snprintf(status_action_label_, sizeof(status_action_label_), "Arming%s", ellipses[animation_counter_>>1]);
          settings_.apply_value(AT_SETTING_ACTION, AUTOTUNER_RUN);
          break;
        case DAC_VOLT_0_BASELINE:
          snprintf(status_action_label_, sizeof(status_action_label_), "0V baseline%s", ellipses[animation_counter_>>1]);
          settings_.apply_value(AT_SETTING_ACTION, AUTOTUNER_STOP);
          break;
        case DAC_VOLT_TARGET_FREQUENCIES: 
        case AUTO_CALIBRATION_STEP_LAST: {
          auto octaves = owner_->get_octave_cnt();
          auto ptr = status_action_label_;
          while (octaves--) *ptr++ = '.';
          *ptr = '\0';
          //snprintf(status_action_label_, sizeof(status_action_label_), "Setup%s", ellipses[animation_counter_>>1]);
          settings_.apply_value(AT_SETTING_ACTION, AUTOTUNER_STOP);
        }
        break;
        default:
          snprintf(status_action_label_, sizeof(status_action_label_), "Running%s", ellipses[animation_counter_>>1]);
          settings_.apply_value(AT_SETTING_ACTION, AUTOTUNER_STOP);
        break;
        }
      }
    } else {
      snprintf(status_action_label_, sizeof(status_action_label_), "Ready");
      settings_.apply_value(AT_SETTING_ACTION, AUTOTUNER_ARM);
    }

    settings_.apply_value(AT_SETTING_RESET, calibration_data_->is_valid() ? AUTOTUNER_USE : AUTOTUNER_RESET);
  }

  template <typename Owner>
  void Autotuner<Owner>::Tick() {
    ++ticks_;
    if (ticks_ > kAnimationTicks) {
      animation_counter_ = (animation_counter_ + 1) & 0x7;
      ticks_ = 0;
    }
  }

  template <typename Owner>
  void Autotuner<Owner>::Draw() const {

    menu::DefaultTitleBar::Draw();
    graphics.printf("%s ", Strings::channel_id[channel_]);

    auto step = owner_->autotuner_step();
    if (step >= DAC_VOLT_3m && step <= DAC_VOLT_6) {
      graphics.print(AT_steps[step - DAC_VOLT_3m]);
      graphics.print(' ');
    }

    if (owner_->autotuner_ready()) {
      char freqstr[12];
      snprintf(freqstr, sizeof(freqstr), "%7.3f", owner_->get_auto_frequency());
      graphics.setPrintPos(128, 2);
      graphics.print_right(freqstr);
    }

    menu::SettingsList<menu::kScreenLines, 0, menu::kDefaultValueX> settings_list(cursor_);
    menu::SettingsListItem list_item;
    while (settings_list.available()) {
      const int setting = settings_list.Next(list_item);
      const int value = settings_.get_value(setting);
      const auto &attr = AutotunerSettings::value_attributes(setting);

      if (AT_SETTING_ACTION == setting) {
        list_item.DrawCustomName(status_action_label_, value, attr);
      } else if (AT_SETTING_RESET == setting) {
        if (calibration_data_->is_valid()) {
          list_item.DrawCharName("Slot empty");
          list_item.DrawCustom();
        } else {
          list_item.DrawDefault(value, attr);
        }
      } else {
        list_item.DrawDefault(value, attr);
      }
    }
  }
  
  template <typename Owner>
  void Autotuner<Owner>::HandleButtonEvent(const UI::Event &event) {
    
     if (UI::EVENT_BUTTON_PRESS == event.type) {
      switch (event.control) {
        case CONTROL_BUTTON_UP: handleButtonUp(event); break;
        case CONTROL_BUTTON_DOWN: handleButtonDown(event); break;
        case CONTROL_BUTTON_L: handleButtonLeft(event); break;    
        case CONTROL_BUTTON_R: handleButtonRight(event); break;
          break;
        default:
          break;
      }
    }
    else if (UI::EVENT_BUTTON_LONG_PRESS == event.type) {
       switch (event.control) {
        case CONTROL_BUTTON_UP:
          // screensaver 
        break;
        case CONTROL_BUTTON_DOWN:
          global_settings.autotune_calibration_data.Reset();
        break;
        case CONTROL_BUTTON_L: 
        break;
        case CONTROL_BUTTON_R:
         // app menu
        break;  
        default:
        break;
      }
    }
  }
  
  template <typename Owner>
  void Autotuner<Owner>::HandleEncoderEvent(const UI::Event &event) {
   
    if (CONTROL_ENCODER_R == event.control) {
      if (cursor_.editing())
        settings_.change_value(cursor_.cursor_pos(), event.value);
      else if (settings_.get_status_action() == AUTOTUNER_ARM)
        cursor_.Scroll(event.value);
      else
        DoAction(AUTOTUNER_STOP);
    } else if (CONTROL_ENCODER_L == event.control) {
      DoAction(AUTOTUNER_STOP);
    }
  }

  template <typename Owner>
  void Autotuner<Owner>::handleButtonUp(const UI::Event &event) {
    if (settings_.get_status_action() == AUTOTUNER_ARM)
      DoAction(AUTOTUNER_STOP);
    else
      Close();
  }
  
  template <typename Owner>
  void Autotuner<Owner>::handleButtonDown(const UI::Event &event) {
    if (settings_.get_status_action() == AUTOTUNER_ARM)
      DoAction(AUTOTUNER_STOP);
    else
      Close();
  }
  
  template <typename Owner>
  void Autotuner<Owner>::handleButtonLeft(const UI::Event &) {
    Close();
  }

  template <typename Owner>
  void Autotuner<Owner>::handleButtonRight(const UI::Event &event) {
    switch(cursor_.cursor_pos()) {
    case AT_SETTING_RESET: DoReset(settings_.get_reset_action()); break;
    case AT_SETTING_ACTION: DoAction(settings_.get_status_action()); break;
    default:
      cursor_.toggle_editing();
    break;
    }
  }

} // namespace OC

#endif // OC_AUTOTUNER_H
