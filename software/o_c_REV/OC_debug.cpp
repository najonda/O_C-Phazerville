#include <Arduino.h>
#include "OC_ADC.h"
#include "OC_app_switcher.h"
#include "OC_config.h"
#include "OC_core.h"
#include "OC_debug.h"
#include "OC_menus.h"
#include "OC_ui.h"
#include "OC_version.h"
#include "util/util_misc.h"
#include "src/extern/dspinst.h"

namespace OC {

namespace DEBUG {
  debug::AveragedCycles ISR_cycles;
  debug::AveragedCycles UI_cycles;
  debug::AveragedCycles MENU_draw_cycles;
  uint32_t UI_event_count;
  uint32_t UI_max_queue_depth;
  uint32_t UI_queue_overflow;

  void Init() {
    debug::CycleMeasurement::Init();
    DebugPins::Init();
  }
}; // namespace DEBUG

static void debug_menu_core() {

  graphics.setPrintPos(2, 12);
  graphics.printf("%uMHz %uus+%uus", F_CPU / 1000 / 1000, OC_CORE_TIMER_RATE, OC_UI_TIMER_RATE);
  
  graphics.setPrintPos(2, 22);
  uint32_t isr_us = debug::cycles_to_us(DEBUG::ISR_cycles.value());
  graphics.printf("CORE%3u/%3u/%3u %2u%%",
                  debug::cycles_to_us(DEBUG::ISR_cycles.min_value()),
                  isr_us,
                  debug::cycles_to_us(DEBUG::ISR_cycles.max_value()),
                  (isr_us * 100) /  OC_CORE_TIMER_RATE);

  graphics.setPrintPos(2, 32);
  graphics.printf("POLL%3u/%3u/%3u",
                  debug::cycles_to_us(DEBUG::UI_cycles.min_value()),
                  debug::cycles_to_us(DEBUG::UI_cycles.value()),
                  debug::cycles_to_us(DEBUG::UI_cycles.max_value()));

#ifdef OC_DEBUG_UI
  graphics.setPrintPos(2, 42);
  graphics.printf("UI   !%u #%u", DEBUG::UI_queue_overflow, DEBUG::UI_event_count);
  graphics.setPrintPos(2, 52);
#endif
}

static void debug_menu_version() {
  weegfx::coord_t y = 12;
  graphics.drawStr(2, y, OC_VERSION); y += 10;
#ifdef IO_10V
  graphics.drawStr(2, y, "IO_1OV"); y += 10;
#endif
#ifdef BUCHLA_SUPPORT
  graphics.drawStr(2, y, "BUCHLA_SUPPORT"); y += 10;
#endif
#ifdef BUCHLA_cOC
  graphics.drawStr(2, y, "BUCHLA_cOC"); y += 10;
#endif
#ifdef BUCHLA_4U
  graphics.drawStr(2, y, "BUCHLA_4U"); y += 10;
#endif
}

static void debug_menu_gfx() {
  graphics.drawFrame(0, 0, 128, 64);

  graphics.setPrintPos(0, 12);
  graphics.print("W");

  graphics.setPrintPos(2, 22);
  graphics.printf("MENU %3u/%3u/%3u",
                  debug::cycles_to_us(DEBUG::MENU_draw_cycles.min_value()),
                  debug::cycles_to_us(DEBUG::MENU_draw_cycles.value()),
                  debug::cycles_to_us(DEBUG::MENU_draw_cycles.max_value()));
}

static void debug_menu_adc() {
  graphics.setPrintPos(2, 12);
  graphics.printf("CV1 %5d %5u", ADC::value<ADC_CHANNEL_1>(), ADC::raw_value(ADC_CHANNEL_1));

  graphics.setPrintPos(2, 22);
  graphics.printf("CV2 %5d %5u", ADC::value<ADC_CHANNEL_2>(), ADC::raw_value(ADC_CHANNEL_2));

  graphics.setPrintPos(2, 32);
  graphics.printf("CV3 %5d %5u", ADC::value<ADC_CHANNEL_3>(), ADC::raw_value(ADC_CHANNEL_3));

  graphics.setPrintPos(2, 42);
  graphics.printf("CV4 %5d %5u", ADC::value<ADC_CHANNEL_4>(), ADC::raw_value(ADC_CHANNEL_4));
}

#ifdef OC_DEBUG_ADC_STATS
static void debug_menu_adc2() {
  weegfx::coord_t y = 12;
  for (int channel = ADC_CHANNEL_1; channel < ADC_CHANNEL_LAST; ++channel) {
    auto &stats = ADC::get_channel_stats(static_cast<ADC_CHANNEL>(channel));
    graphics.setPrintPos(2, y);
    graphics.printf("CV%d %5d %5d", channel + 1, stats.min, stats.max);
    y += 10;
  }
}
#endif

static void debug_menu_app() {
  auto app = app_switcher.current_app();
  if (app) {
    graphics.print(app->name());
    app->DrawDebugInfo();
  } else {
    graphics.print("?");
  }
}

struct DebugMenu {
  const char *title;
  void (*display_fn)();
};

static const DebugMenu debug_menus[] = {
  { " CORE", debug_menu_core },
  { " VERSION", debug_menu_version },
  { " GFX", debug_menu_gfx },
  { " ADC", debug_menu_adc },
#ifdef OC_DEBUG_ADC_STATS
  { " ADC min/max", debug_menu_adc2 },
#endif
  { " ", debug_menu_app },
};

void Ui::DebugStats() {
  SERIAL_PRINTLN("DEBUG/STATS MENU");

  int current_menu_index = 0;
  bool exit_loop = false;
  while (!exit_loop) {
    const auto &current_menu = debug_menus[current_menu_index];

    GRAPHICS_BEGIN_FRAME(false);
      graphics.setPrintPos(2, 2);
      graphics.printf("%d/%u", current_menu_index + 1, ARRAY_SIZE(debug_menus));
      graphics.print(current_menu.title);
      current_menu.display_fn();
    GRAPHICS_END_FRAME();

    while (event_queue_.available()) {
      UI::Event event = event_queue_.PullEvent();
      if (UI::EVENT_ENCODER == event.type && CONTROL_ENCODER_L == event.control) {
        current_menu_index = current_menu_index + event.value;
      } else if (CONTROL_BUTTON_R == event.control) {
        exit_loop = true;
      } else if (CONTROL_BUTTON_L == event.control) {
        ++current_menu_index;
      }
    }
    CONSTRAIN(current_menu_index, 0, (int)ARRAY_SIZE(debug_menus) - 1);
  }

  event_queue_.Flush();
  event_queue_.Poke();
}

} // namespace OC
