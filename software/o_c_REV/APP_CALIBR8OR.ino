// Copyright (c) 2023, Nicholas J. Michalek
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

#include "HSApplication.h"
#include "HSMIDI.h"
#include "util/util_settings.h"
#include "braids_quantizer.h"
#include "braids_quantizer_scales.h"
#include "OC_scales.h"

#define CAL8_MAX_TRANSPOSE 60
const int CAL8OR_PRECISION = 10000;

// Settings storage spec (per channel?)
enum CAL8SETTINGS {
    CAL8_SCALE_A, // 12 bits
    CAL8_SCALEFACTOR_A, // 10 bits
    CAL8_OFFSET_A, // 8 bits
    CAL8_TRANSPOSE_A, // 8 bits
    CAL8_CLOCKMODE_A, // 2 bits

    CAL8_SCALE_B, // 12 bits
    CAL8_SCALEFACTOR_B, // 10 bits
    CAL8_OFFSET_B, // 8 bits
    CAL8_TRANSPOSE_B, // 8 bits
    CAL8_CLOCKMODE_B, // 2 bits

    CAL8_SCALE_C, // 12 bits
    CAL8_SCALEFACTOR_C, // 10 bits
    CAL8_OFFSET_C, // 8 bits
    CAL8_TRANSPOSE_C, // 8 bits
    CAL8_CLOCKMODE_C, // 2 bits

    CAL8_SCALE_D, // 12 bits
    CAL8_SCALEFACTOR_D, // 10 bits
    CAL8_OFFSET_D, // 8 bits
    CAL8_TRANSPOSE_D, // 8 bits
    CAL8_CLOCKMODE_D, // 2 bits

    CAL8_SETTING_LAST
};

class Calibr8or : public HSApplication,
    public settings::SettingsBase<Calibr8or, CAL8_SETTING_LAST> {
public:
    enum Cal8Channel {
        CAL8_CHANNEL_A,
        CAL8_CHANNEL_B,
        CAL8_CHANNEL_C,
        CAL8_CHANNEL_D,

        NR_OF_CHANNELS
    };
	enum Cal8EditMode {
        TRANSPOSE,
        TRACKING,

        NR_OF_EDITMODES
    };
    enum Cal8ClockMode {
        CONTINUOUS,
        TRIG_TRANS,
        SAMPLE_AND_HOLD,

        NR_OF_CLOCKMODES
    };

	void Start() {
        for (int ch = 0; ch < NR_OF_CHANNELS; ++ch) {
            quantizer[ch].Init();
            scale[ch] = OC::Scales::SCALE_SEMI;
            quantizer[ch].Configure(OC::Scales::GetScale(scale[ch]), 0xffff);
        }
	}
	
	void Resume() {
        LoadFromEEPROM();
	}

    void Controller() {
        for (int ch = 0; ch < NR_OF_CHANNELS; ++ch) {
            bool clocked = Clock(ch);

            // clocked transpose
            if (CONTINUOUS == clocked_mode[ch] || clocked) {
                transpose_active[ch] = transpose[ch];
            }

            // respect S&H mode
            if (clocked_mode[ch] != SAMPLE_AND_HOLD || clocked) {
                // CV value
                int pitch = In(ch);
                int quantized = quantizer[ch].Process(pitch, 0, transpose_active[ch]);
                last_note[ch] = quantized;
            }

            int output_cv = last_note[ch] * (CAL8OR_PRECISION + scale_factor[ch]) / CAL8OR_PRECISION;
            output_cv += offset[ch];

            Out(ch, output_cv);
        }
    }

    void View() {
        gfxHeader("Calibr8or");
        DrawInterface();
    }

    void SaveToEEPROM() {
        int ix = 0;

        for (int ch = 0; ch < NR_OF_CHANNELS; ++ch) {
            values_[ix++] = scale[ch];
            values_[ix++] = scale_factor[ch] + 500;
            values_[ix++] = offset[ch] + 63;
            values_[ix++] = transpose[ch] + CAL8_MAX_TRANSPOSE;
            values_[ix++] = clocked_mode[ch];
        }
    }
    /*
    CAL8_SCALE_A, // 12 bits
    CAL8_SCALEFACTOR_A, // 10 bits
    CAL8_OFFSET_A, // 8 bits
    CAL8_TRANSPOSE_A, // 8 bits
    CAL8_CLOCKMODE_A, // 2 bits
    */
    void LoadFromEEPROM() {
        int ix = 0;

        for (int ch = 0; ch < NR_OF_CHANNELS; ++ch) {
            int scale_ = values_[ix++];
            scale[ch] = constrain(scale_, 0, OC::Scales::NUM_SCALES - 1);
            scale_factor[ch] = values_[ix++] - 500;
            offset[ch] = values_[ix++] - 63;
            transpose[ch] = values_[ix++] - CAL8_MAX_TRANSPOSE;
            clocked_mode[ch] = values_[ix++];
        }
    }

    /////////////////////////////////////////////////////////////////
    // Control handlers
    /////////////////////////////////////////////////////////////////
    void OnLeftButtonPress() {
        // Toggle between Transpose mode and Tracking Compensation
        ++edit_mode %= NR_OF_EDITMODES;
    }

    void OnLeftButtonLongPress() {
        // Toggle triggered transpose mode
        ++clocked_mode[sel_chan] %= NR_OF_CLOCKMODES;
    }

    void OnRightButtonPress() {
        // Scale selection
        scale_edit = !scale_edit;
    }

    void OnUpButtonPress() {
        ++sel_chan %= NR_OF_CHANNELS;
    }

    void OnDownButtonPress() {
        if (--sel_chan < 0) sel_chan += NR_OF_CHANNELS;
    }

    void OnDownButtonLongPress() {
    }

    // Left encoder: Octave or VScaling
    void OnLeftEncoderMove(int direction) {
        if (edit_mode == TRANSPOSE) { // Octave jump
            int s = OC::Scales::GetScale(scale[sel_chan]).num_notes;
            transpose[sel_chan] += (direction * s);
            while (transpose[sel_chan] > CAL8_MAX_TRANSPOSE) transpose[sel_chan] -= s;
            while (transpose[sel_chan] < -CAL8_MAX_TRANSPOSE) transpose[sel_chan] += s;
        }
        else { // Tracking compensation
            scale_factor[sel_chan] = constrain(scale_factor[sel_chan] + direction, -500, 500);
        }
    }

    // Right encoder: Semitones or Bias Offset + Scale Select
    void OnRightEncoderMove(int direction) {
        if (scale_edit) {
            scale[sel_chan] += direction;
            if (scale[sel_chan] >= OC::Scales::NUM_SCALES) scale[sel_chan] = 0;
            if (scale[sel_chan] < 0) scale[sel_chan] = OC::Scales::NUM_SCALES - 1;
            quantizer[sel_chan].Configure(OC::Scales::GetScale(scale[sel_chan]), 0xffff);
            return;
        }

        if (edit_mode == TRANSPOSE) {
            transpose[sel_chan] = constrain(transpose[sel_chan] + direction, -CAL8_MAX_TRANSPOSE, CAL8_MAX_TRANSPOSE);
        }
        else {
            offset[sel_chan] = constrain(offset[sel_chan] + direction, -63, 64);
        }
    }

private:
	int sel_chan = 0;
    int edit_mode = 0; // Cal8EditMode
    bool scale_edit = 0;

    braids::Quantizer quantizer[NR_OF_CHANNELS];
    int scale[NR_OF_CHANNELS]; // Scale per channel
    int last_note[NR_OF_CHANNELS]; // for S&H mode

    int clocked_mode[NR_OF_CHANNELS];
    int scale_factor[NR_OF_CHANNELS] = {0,0,0,0}; // precision of 0.01% as an offset from 100%
    int offset[NR_OF_CHANNELS] = {0,0,0,0}; // fine-tuning offset
    int transpose[NR_OF_CHANNELS] = {0,0,0,0}; // in semitones
    int transpose_active[NR_OF_CHANNELS] = {0,0,0,0}; // held value while waiting for trigger

    void DrawInterface() {
        // Draw channel tabs
        for (int i = 0; i < NR_OF_CHANNELS; ++i) {
            gfxLine(i*32, 13, i*32, 22); // vertical line on left
            if (clocked_mode[i]) gfxIcon(2 + i*32, 14, CLOCK_ICON);
            if (clocked_mode[i] == SAMPLE_AND_HOLD) gfxIcon(22 + i*32, 14, STAIRS_ICON);
            gfxPrint(i*32 + 13, 14, i+1);

            if (i == sel_chan)
                gfxInvert(1 + i*32, 13, 31, 11);
        }
        gfxLine(127, 13, 127, 22); // vertical line
        gfxLine(0, 23, 127, 23);

        // Draw parameters for selected channel
        int y = 30;

        // Transpose
        gfxIcon(10, y, BEND_ICON);
        graphics.setPrintPos(20, y);

        if (transpose[sel_chan] >= 0)
            gfxPrint("+");
        else
            gfxPrint("-");

        int s = OC::Scales::GetScale(scale[sel_chan]).num_notes;
        int octave = transpose[sel_chan] / s;
        int semitone = transpose[sel_chan] % s;
        gfxPrint(abs(octave));
        gfxPrint(".");
        gfxPrint(abs(semitone));

        // Scale
        gfxIcon(60, y, SCALE_ICON);
        gfxPrint(70, y, OC::scale_names_short[scale[sel_chan]]);
        if (scale_edit) gfxInvert(69, y-1, 30, 9);

        // Tracking Compensation
        y += 20;
        gfxIcon(10, y, ZAP_ICON);
        int whole = (scale_factor[sel_chan] + CAL8OR_PRECISION) / 100;
        int decimal = (scale_factor[sel_chan] + CAL8OR_PRECISION) % 100;
        gfxPrint(20 + pad(100, whole), y, whole);
        gfxPrint(".");
        if (decimal < 10) gfxPrint("0");
        gfxPrint(decimal);
        gfxPrint("% ");

        if (offset[sel_chan] >= 0) gfxPrint("+");
        gfxPrint(offset[sel_chan]);

        // mode indicator
        gfxIcon(0, 30 + edit_mode*20, RIGHT_ICON);
    }
};

SETTINGS_DECLARE(Calibr8or, CAL8_SETTING_LAST) {
    {0, 0, 65535, "Scale A", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "CV Scaling Factor A", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 255, "Offset Bias A", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 255, "Transpose A", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 3, "Clock Mode A", NULL, settings::STORAGE_TYPE_U4},

    {0, 0, 65535, "Scale B", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "CV Scaling Factor B", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 255, "Offset Bias B", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 255, "Transpose B", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 3, "Clock Mode B", NULL, settings::STORAGE_TYPE_U4},

    {0, 0, 65535, "Scale C", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "CV Scaling Factor C", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 255, "Offset Bias C", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 255, "Transpose C", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 3, "Clock Mode C", NULL, settings::STORAGE_TYPE_U4},

    {0, 0, 65535, "Scale D", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "CV Scaling Factor D", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 255, "Offset Bias D", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 255, "Transpose D", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 3, "Clock Mode D", NULL, settings::STORAGE_TYPE_U4}
};

Calibr8or Calibr8or_instance;

// App stubs
void Calibr8or_init() { Calibr8or_instance.BaseStart(); }

size_t Calibr8or_storageSize() {
    return Calibr8or::storageSize();
}
size_t Calibr8or_save(void *storage) {
    return Calibr8or_instance.Save(storage);
}
size_t Calibr8or_restore(const void *storage) {
    size_t s = Calibr8or_instance.Restore(storage);
    Calibr8or_instance.Resume();
    return s;
}

void Calibr8or_isr() { return Calibr8or_instance.BaseController(); }

void Calibr8or_handleAppEvent(OC::AppEvent event) {
    switch (event) {
    case OC::APP_EVENT_RESUME:
        Calibr8or_instance.Resume();
        break;

    // The idea is to auto-save when the screen times out...
    case OC::APP_EVENT_SUSPEND:
    case OC::APP_EVENT_SCREENSAVER_ON:
        Calibr8or_instance.SaveToEEPROM();
        // TODO: initiate actual EEPROM save
        // app_data_save();
        break;

    default: break;
    }
}

void Calibr8or_loop() {} // Deprecated

void Calibr8or_menu() { Calibr8or_instance.BaseView(); }

void Calibr8or_screensaver() {
    // XXX: Consider a view like Quantermain
    // other ideas: Actual note being played, current transpose setting
    // ...for all 4 channels at once.
}

void Calibr8or_handleButtonEvent(const UI::Event &event) {
    // For left encoder, handle press and long press
    if (event.control == OC::CONTROL_BUTTON_L) {
        if (event.type == UI::EVENT_BUTTON_LONG_PRESS) Calibr8or_instance.OnLeftButtonLongPress();
        else Calibr8or_instance.OnLeftButtonPress();
    }

    // For right encoder, only handle press (long press is reserved)
    if (event.control == OC::CONTROL_BUTTON_R && event.type == UI::EVENT_BUTTON_PRESS) Calibr8or_instance.OnRightButtonPress();

    // For up button, handle only press (long press is reserved)
    if (event.control == OC::CONTROL_BUTTON_UP) Calibr8or_instance.OnUpButtonPress();

    // For down button, handle press and long press
    if (event.control == OC::CONTROL_BUTTON_DOWN) {
        if (event.type == UI::EVENT_BUTTON_PRESS) Calibr8or_instance.OnDownButtonPress();
        if (event.type == UI::EVENT_BUTTON_LONG_PRESS) Calibr8or_instance.OnDownButtonLongPress();
    }
}

void Calibr8or_handleEncoderEvent(const UI::Event &event) {
    // Left encoder turned
    if (event.control == OC::CONTROL_ENCODER_L) Calibr8or_instance.OnLeftEncoderMove(event.value);

    // Right encoder turned
    if (event.control == OC::CONTROL_ENCODER_R) Calibr8or_instance.OnRightEncoderMove(event.value);
}
