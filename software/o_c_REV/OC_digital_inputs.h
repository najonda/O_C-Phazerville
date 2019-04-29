#ifndef OC_DIGITAL_INPUTS_H_
#define OC_DIGITAL_INPUTS_H_

#include <Arduino.h>
#include <stdint.h>
#include "OC_config.h"
#include "OC_core.h"
#include "OC_gpio.h"

namespace OC {

enum DigitalInput {
  DIGITAL_INPUT_1,
  DIGITAL_INPUT_2,
  DIGITAL_INPUT_3,
  DIGITAL_INPUT_4,
  DIGITAL_INPUT_LAST
};

#define DIGITAL_INPUT_MASK(x) (0x1 << (x))

static constexpr uint32_t DIGITAL_INPUT_1_MASK = DIGITAL_INPUT_MASK(DIGITAL_INPUT_1);
static constexpr uint32_t DIGITAL_INPUT_2_MASK = DIGITAL_INPUT_MASK(DIGITAL_INPUT_2);
static constexpr uint32_t DIGITAL_INPUT_3_MASK = DIGITAL_INPUT_MASK(DIGITAL_INPUT_3);
static constexpr uint32_t DIGITAL_INPUT_4_MASK = DIGITAL_INPUT_MASK(DIGITAL_INPUT_4);

template <DigitalInput> struct InputPinDesc { };
template <> struct InputPinDesc<DIGITAL_INPUT_1> { static constexpr int PIN = TR1; };
template <> struct InputPinDesc<DIGITAL_INPUT_2> { static constexpr int PIN = TR2; };
template <> struct InputPinDesc<DIGITAL_INPUT_3> { static constexpr int PIN = TR3; };
template <> struct InputPinDesc<DIGITAL_INPUT_4> { static constexpr int PIN = TR4; };

class DigitalInputs {
public:

  static void Init();

  static void reInit();

  static void Scan();

  // @return mask of all pins cloked since last call
  static inline uint32_t rising_edges() {
    return rising_edges_;
  }

  // @return mask of all pins that are raised (at last Scan)
  static inline uint32_t raised_mask() {
    return raised_mask_;
  }

  template <DigitalInput input> static inline bool read_immediate() {
    return !digitalReadFast(InputPinDesc<input>::PIN);
  }

  static inline bool read_immediate(DigitalInput input) {
    return !digitalReadFast(InputPinMap(input));
  }

  template <DigitalInput input> static inline void capture() {
    captures_[input] = 1;
  }

private:

  inline static int InputPinMap(DigitalInput input) {
    static constexpr int kPinMap[] = {
      InputPinDesc<DIGITAL_INPUT_1>::PIN, InputPinDesc<DIGITAL_INPUT_2>::PIN,
      InputPinDesc<DIGITAL_INPUT_3>::PIN, InputPinDesc<DIGITAL_INPUT_4>::PIN
    };
    return kPinMap[input];
  }

  static uint32_t rising_edges_;
  static uint32_t raised_mask_;
  static volatile uint32_t captures_[DIGITAL_INPUT_LAST];

  template <DigitalInput input>
  static uint32_t ScanInput() {
    if (captures_[input]) {
      captures_[input] = 0;
      return DIGITAL_INPUT_MASK(input);
    } else {
      return 0;
    }
  }
};

// Helper class for visualizing digital inputs with decay
// Uses 4 bits for decay
class DigitalInputDisplay {
public:
  static constexpr uint32_t kDisplayTime = OC_CORE_ISR_FREQ / 8;
  static constexpr uint32_t kPhaseInc = (0xf << 28) / kDisplayTime;

  void Init() {
    phase_ = 0;
  }

  void Update(uint32_t ticks, bool clocked) {
    uint32_t phase_inc = ticks * kPhaseInc;
    if (clocked) {
      phase_ = 0xffffffff;
    } else {
      uint32_t phase = phase_;
      if (phase) {
        if (phase < phase_inc)
          phase_ = 0;
        else
          phase_ = phase - phase_inc;
      }
    }
  }

  uint8_t getState() const {
    return phase_ >> 28;
  }

private:
  uint32_t phase_;
};

};

#endif // OC_DIGITAL_INPUTS_H_
