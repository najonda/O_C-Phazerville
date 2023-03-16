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

/*
 * Based on a design spec from Chris Meyer / Alias Zone / Learning Modular
 */

#if defined(ENABLE_APP_CALIBR8OR) || defined(ENABLE_CALIBR8OR_X4)

#include "HSApplication.h"
#include "HSMIDI.h"
#include "util/util_settings.h"
#include "braids_quantizer.h"
#include "braids_quantizer_scales.h"
#include "OC_scales.h"
#include "SegmentDisplay.h"
#include "src/drivers/FreqMeasure/OC_FreqMeasure.h"

#define CAL8_MAX_TRANSPOSE 60
const int CAL8OR_PRECISION = 10000;

// Settings storage spec (per channel?)
enum CAL8SETTINGS {
    CAL8_DATA_VALID, // 1 bit

    CAL8_SCALE_A, // 12 bits
    CAL8_SCALEFACTOR_A, // 10 bits
    CAL8_OFFSET_A, // 8 bits
    CAL8_TRANSPOSE_A, // 8 bits
    CAL8_ROOTKEY_AND_CLOCKMODE_A, // 4 + 2 bits

    CAL8_SCALE_B,
    CAL8_SCALEFACTOR_B,
    CAL8_OFFSET_B,
    CAL8_TRANSPOSE_B,
    CAL8_ROOTKEY_AND_CLOCKMODE_B,

    CAL8_SCALE_C,
    CAL8_SCALEFACTOR_C,
    CAL8_OFFSET_C,
    CAL8_TRANSPOSE_C,
    CAL8_ROOTKEY_AND_CLOCKMODE_C,

    CAL8_SCALE_D,
    CAL8_SCALEFACTOR_D,
    CAL8_OFFSET_D,
    CAL8_TRANSPOSE_D,
    CAL8_ROOTKEY_AND_CLOCKMODE_D,

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

    void set_index(int index_) { index = index_; }

	void Start() {
        for (int ch = 0; ch < NR_OF_CHANNELS; ++ch) {
            quantizer[ch].Init();
            scale[ch] = OC::Scales::SCALE_SEMI;
            quantizer[ch].Configure(OC::Scales::GetScale(scale[ch]), 0xffff);

            scale_factor[ch] = 0;
            offset[ch] = 0;
            transpose[ch] = 0;
            clocked_mode[ch] = 0;
            last_note[ch] = 0;
        }

        segment.Init(SegmentSize::BIG_SEGMENTS);

        // make sure to turn this off, just in case?
        FreqMeasure.end();
        OC::DigitalInputs::reInit();
	}
	
	void Resume() {
        if (values_[CAL8_DATA_VALID])
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
                int quantized = quantizer[ch].Process(pitch, root_note[ch] * 128, transpose_active[ch]);
                last_note[ch] = quantized;
            }

            int output_cv = last_note[ch] * (CAL8OR_PRECISION + scale_factor[ch]) / CAL8OR_PRECISION;
            output_cv += offset[ch];

            Out(ch, output_cv);
        }
    }

    void View() {
        gfxHeader("Calibr8or");
#ifdef ENABLE_CALIBR8OR_X4
        const char * cal8_preset_id[4] = {"A", "B", "C", "D"};
        gfxPrint(120, 0, cal8_preset_id[index]);
#endif
        DrawInterface();
    }

    void SaveToEEPROM() {
        int ix = 0;

        values_[ix++] = 1; // validity flag

        for (int ch = 0; ch < NR_OF_CHANNELS; ++ch) {
            values_[ix++] = scale[ch];
            values_[ix++] = scale_factor[ch] + 500;
            values_[ix++] = offset[ch] + 63;
            values_[ix++] = transpose[ch] + CAL8_MAX_TRANSPOSE;
            values_[ix++] = ((clocked_mode[ch] & 0x03) << 4) | (root_note[ch] & 0x0f);
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
        int ix = 1; // skip validity flag

        for (int ch = 0; ch < NR_OF_CHANNELS; ++ch) {
            scale[ch] = values_[ix++];
            scale[ch] = constrain(scale[ch], 0, OC::Scales::NUM_SCALES - 1);
            quantizer[ch].Configure(OC::Scales::GetScale(scale[ch]), 0xffff);

            scale_factor[ch] = values_[ix++] - 500;
            offset[ch] = values_[ix++] - 63;
            transpose[ch] = values_[ix++] - CAL8_MAX_TRANSPOSE;

            uint32_t root_and_mode = uint32_t(values_[ix++]);
            clocked_mode[ch] = ((root_and_mode >> 4) & 0x03) % NR_OF_CLOCKMODES;
            root_note[ch] = constrain(int(root_and_mode & 0x0f), 0, 11);
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

    // Left encoder: Octave or VScaling + Root Note
    void OnLeftEncoderMove(int direction) {
        if (scale_edit) {
            root_note[sel_chan] = constrain(root_note[sel_chan] + direction, 0, 11);
            return;
        }

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
    int index = 0;

	int sel_chan = 0;
    int edit_mode = 0; // Cal8EditMode
    bool scale_edit = 0;

    SegmentDisplay segment;
    braids::Quantizer quantizer[NR_OF_CHANNELS];
    int scale[NR_OF_CHANNELS]; // Scale per channel
    int root_note[NR_OF_CHANNELS]; // in semitones from C
    int last_note[NR_OF_CHANNELS]; // for S&H mode

    uint8_t clocked_mode[NR_OF_CHANNELS];
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
        int y = 32;

        // Transpose
        gfxIcon(9, y, BEND_ICON);

        // -- LCD Display Section --
        gfxFrame(20, y-3, 64, 18);
        gfxIcon(23, y+2, (transpose[sel_chan] >= 0)? PLUS_ICON : MINUS_ICON);

        int s = OC::Scales::GetScale(scale[sel_chan]).num_notes;
        int octave = transpose[sel_chan] / s;
        int semitone = transpose[sel_chan] % s;
        segment.PrintWhole(33, y, abs(octave), 10);
        gfxPrint(53, y+5, ".");
        segment.PrintWhole(61, y, abs(semitone), 10);

        // Scale
        gfxIcon(89, y, SCALE_ICON);
        gfxPrint(99, y, OC::scale_names_short[scale[sel_chan]]);
        if (scale_edit) {
            gfxInvert(98, y-1, 29, 9);
            gfxIcon(100, y+10, RIGHT_ICON);
        }
        // Root Note
        gfxPrint(110, y+10, OC::Strings::note_names_unpadded[root_note[sel_chan]]);

        // Tracking Compensation
        y += 22;
        gfxIcon(9, y, ZAP_ICON);
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
        if (!scale_edit)
            gfxIcon(0, 32 + edit_mode*22, RIGHT_ICON);
    }
};

SETTINGS_DECLARE(Calibr8or, CAL8_SETTING_LAST) {
    {0, 0, 1, "validity flag", NULL, settings::STORAGE_TYPE_U4},

    {0, 0, 65535, "Scale A", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "CV Scaling Factor A", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 255, "Offset Bias A", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 255, "Transpose A", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 255, "Root Key + Mode A", NULL, settings::STORAGE_TYPE_U8},

    {0, 0, 65535, "Scale B", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "CV Scaling Factor B", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 255, "Offset Bias B", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 255, "Transpose B", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 255, "Root Key + Mode B", NULL, settings::STORAGE_TYPE_U8},

    {0, 0, 65535, "Scale C", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "CV Scaling Factor C", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 255, "Offset Bias C", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 255, "Transpose C", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 255, "Root Key + Mode C", NULL, settings::STORAGE_TYPE_U8},

    {0, 0, 65535, "Scale D", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "CV Scaling Factor D", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 255, "Offset Bias D", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 255, "Transpose D", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 255, "Root Key + Mode D", NULL, settings::STORAGE_TYPE_U8}
};

// To allow FOUR preset configs... I just made 4 copies of everything lol
Calibr8or Calibr8or_instance[4];

// App stubs
void Calibr8or_init() { Calibr8or_instance[0].BaseStart(); }
void Calibr8or_init(int index) {
    Calibr8or_instance[index].BaseStart();
    Calibr8or_instance[index].set_index(index);
}
void Calibr8orA_init() { Calibr8or_init(0); }
void Calibr8orB_init() { Calibr8or_init(1); }
void Calibr8orC_init() { Calibr8or_init(2); }
void Calibr8orD_init() { Calibr8or_init(3); }

size_t Calibr8or_storageSize() { return Calibr8or::storageSize(); }
size_t Calibr8orA_storageSize() { return Calibr8or::storageSize(); }
size_t Calibr8orB_storageSize() { return Calibr8or::storageSize(); }
size_t Calibr8orC_storageSize() { return Calibr8or::storageSize(); }
size_t Calibr8orD_storageSize() { return Calibr8or::storageSize(); }

size_t Calibr8or_save(void *storage) { return Calibr8or_instance[0].Save(storage); }
size_t Calibr8orA_save(void *storage) { return Calibr8or_instance[0].Save(storage); }
size_t Calibr8orB_save(void *storage) { return Calibr8or_instance[1].Save(storage); }
size_t Calibr8orC_save(void *storage) { return Calibr8or_instance[2].Save(storage); }
size_t Calibr8orD_save(void *storage) { return Calibr8or_instance[3].Save(storage); }

size_t Calibr8or_restore(const void *storage, int index) {
    size_t s = Calibr8or_instance[index].Restore(storage);
    Calibr8or_instance[index].Resume();
    return s;
}
size_t Calibr8or_restore(const void *storage) { return Calibr8or_restore(storage, 0); }
size_t Calibr8orA_restore(const void *storage) { return Calibr8or_restore(storage, 0); }
size_t Calibr8orB_restore(const void *storage) { return Calibr8or_restore(storage, 1); }
size_t Calibr8orC_restore(const void *storage) { return Calibr8or_restore(storage, 2); }
size_t Calibr8orD_restore(const void *storage) { return Calibr8or_restore(storage, 3); }

void Calibr8or_isr() { return Calibr8or_instance[0].BaseController(); }
void Calibr8orA_isr() { return Calibr8or_instance[0].BaseController(); }
void Calibr8orB_isr() { return Calibr8or_instance[1].BaseController(); }
void Calibr8orC_isr() { return Calibr8or_instance[2].BaseController(); }
void Calibr8orD_isr() { return Calibr8or_instance[3].BaseController(); }

void Calibr8or_handleAppEvent(OC::AppEvent event, int index) {
    switch (event) {
    case OC::APP_EVENT_RESUME:
        Calibr8or_instance[index].Resume();
        break;

    // The idea is to auto-save when the screen times out...
    case OC::APP_EVENT_SUSPEND:
    case OC::APP_EVENT_SCREENSAVER_ON:
        Calibr8or_instance[index].SaveToEEPROM();
        // TODO: initiate actual EEPROM save
        // app_data_save();
        break;

    default: break;
    }
}
void Calibr8or_handleAppEvent(OC::AppEvent event) {
    Calibr8or_handleAppEvent(event, 0);
}
void Calibr8orA_handleAppEvent(OC::AppEvent event) { Calibr8or_handleAppEvent(event, 0); }
void Calibr8orB_handleAppEvent(OC::AppEvent event) { Calibr8or_handleAppEvent(event, 1); }
void Calibr8orC_handleAppEvent(OC::AppEvent event) { Calibr8or_handleAppEvent(event, 2); }
void Calibr8orD_handleAppEvent(OC::AppEvent event) { Calibr8or_handleAppEvent(event, 3); }

void Calibr8or_loop() {} // Deprecated
void Calibr8orA_loop() {} // Deprecated
void Calibr8orB_loop() {} // Deprecated
void Calibr8orC_loop() {} // Deprecated
void Calibr8orD_loop() {} // Deprecated

void Calibr8or_menu() { Calibr8or_instance[0].BaseView(); }
void Calibr8orA_menu() { Calibr8or_instance[0].BaseView(); }
void Calibr8orB_menu() { Calibr8or_instance[1].BaseView(); }
void Calibr8orC_menu() { Calibr8or_instance[2].BaseView(); }
void Calibr8orD_menu() { Calibr8or_instance[3].BaseView(); }

void Calibr8or_screensaver() {
    // XXX: Consider a view like Quantermain
    // other ideas: Actual note being played, current transpose setting
    // ...for all 4 channels at once.
}
void Calibr8orA_screensaver() {}
void Calibr8orB_screensaver() {}
void Calibr8orC_screensaver() {}
void Calibr8orD_screensaver() {}

void Calibr8or_handleButtonEvent(const UI::Event &event, int index) {
    // For left encoder, handle press and long press
    if (event.control == OC::CONTROL_BUTTON_L) {
        if (event.type == UI::EVENT_BUTTON_LONG_PRESS) Calibr8or_instance[index].OnLeftButtonLongPress();
        else Calibr8or_instance[index].OnLeftButtonPress();
    }

    // For right encoder, only handle press (long press is reserved)
    if (event.control == OC::CONTROL_BUTTON_R && event.type == UI::EVENT_BUTTON_PRESS) Calibr8or_instance[index].OnRightButtonPress();

    // For up button, handle only press (long press is reserved)
    if (event.control == OC::CONTROL_BUTTON_UP) Calibr8or_instance[index].OnUpButtonPress();

    // For down button, handle press and long press
    if (event.control == OC::CONTROL_BUTTON_DOWN) {
        if (event.type == UI::EVENT_BUTTON_PRESS) Calibr8or_instance[index].OnDownButtonPress();
        if (event.type == UI::EVENT_BUTTON_LONG_PRESS) Calibr8or_instance[index].OnDownButtonLongPress();
    }
}
void Calibr8or_handleButtonEvent(const UI::Event &event) {
    Calibr8or_handleButtonEvent(event, 0);
}
void Calibr8orA_handleButtonEvent(const UI::Event &event) { Calibr8or_handleButtonEvent(event, 0); }
void Calibr8orB_handleButtonEvent(const UI::Event &event) { Calibr8or_handleButtonEvent(event, 1); }
void Calibr8orC_handleButtonEvent(const UI::Event &event) { Calibr8or_handleButtonEvent(event, 2); }
void Calibr8orD_handleButtonEvent(const UI::Event &event) { Calibr8or_handleButtonEvent(event, 3); }

void Calibr8or_handleEncoderEvent(const UI::Event &event, int index) {
    // Left encoder turned
    if (event.control == OC::CONTROL_ENCODER_L) Calibr8or_instance[index].OnLeftEncoderMove(event.value);

    // Right encoder turned
    if (event.control == OC::CONTROL_ENCODER_R) Calibr8or_instance[index].OnRightEncoderMove(event.value);
}
void Calibr8or_handleEncoderEvent(const UI::Event &event) {
    Calibr8or_handleEncoderEvent(event, 0);
}
void Calibr8orA_handleEncoderEvent(const UI::Event &event) { Calibr8or_handleEncoderEvent(event, 0); }
void Calibr8orB_handleEncoderEvent(const UI::Event &event) { Calibr8or_handleEncoderEvent(event, 1); }
void Calibr8orC_handleEncoderEvent(const UI::Event &event) { Calibr8or_handleEncoderEvent(event, 2); }
void Calibr8orD_handleEncoderEvent(const UI::Event &event) { Calibr8or_handleEncoderEvent(event, 3); }

#endif // ENABLE_APP_CALIBR8OR
