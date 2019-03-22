#ifndef OC_AUTOTUNE_H_
#define OC_AUTOTUNE_H_

#include "OC_autotune_presets.h"
#include "OC_DAC.h"

namespace OC {

class AUTOTUNE {
public:
  static void Init();
  static const AutotuneCalibrationData &GetAutotuneCalibrationData(int channel);
};

extern AutotuneCalibrationData auto_calibration_data[DAC_CHANNEL_LAST];
};

#endif // OC_AUTOTUNE_H_
