#ifndef OC_AUTOTUNE_PRESETS_H_
#define OC_AUTOTUNE_PRESETS_H_

#include <Arduino.h>
#include "OC_config.h"

namespace OC {

  struct AutotuneCalibrationData {
 
    uint8_t use_auto_calibration_;
    uint16_t auto_calibrated_octaves[OCTAVES + 1];

    void Reset() {
      memset(this, 0, sizeof(*this));
    }
  };
}

#endif // OC_AUTOTUNE_PRESETS_H_
