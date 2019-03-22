#include "OC_autotune.h"
#include "OC_autotune_presets.h"

namespace OC {

    AutotuneCalibrationData auto_calibration_data[DAC_CHANNEL_LAST];

    /*static*/
    void AUTOTUNE::Init() {
      for (size_t i = 0; i < DAC_CHANNEL_LAST; i++)
        auto_calibration_data[i].Reset();
    }

    /*static*/
    const AutotuneCalibrationData &AUTOTUNE::GetAutotuneCalibrationData(int channel) {
        return auto_calibration_data[channel];
    }
} // namespace OC
