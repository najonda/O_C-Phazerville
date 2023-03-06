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
#include "braids_quantizer.h"
#include "braids_quantizer_scales.h"
#include "OC_scales.h"

#define CAL8_MAX_TRANSPOSE 60

class Calibr8or : public HSApplication, public SystemExclusiveHandler {
public:
    const int CAL8OR_PRECISION = 10000;
	enum Cal8EditMode {
        TRANSPOSE,
        TRACKING,

        NR_OF_EDITMODES
    };
	enum Cal8Channel {
		CHANNEL_A,
		CHANNEL_B,
		CHANNEL_C,
		CHANNEL_D,

		NR_OF_CHANNELS
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
                int quantized = quantizer[ch].Process(In(ch), transpose_active[ch] << 7, 0);
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

    void OnSendSysEx() {
    }

    void OnReceiveSysEx() {
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
        if (edit_mode == TRANSPOSE) {
            transpose[sel_chan] += (direction * 12);
            while (transpose[sel_chan] > CAL8_MAX_TRANSPOSE) transpose[sel_chan] -= 12;
            while (transpose[sel_chan] < -CAL8_MAX_TRANSPOSE) transpose[sel_chan] += 12;
        }
        else { // Tracking compensation
            scale_factor[sel_chan] = constrain(scale_factor[sel_chan] + direction, -500, 500);
        }
    }

    // Right encoder: Semitones or Bias Offset
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
            offset[sel_chan] = constrain(offset[sel_chan] + direction, -100, 100);
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

        int octave = transpose[sel_chan] / 12;
        int semitone = transpose[sel_chan] % 12;
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

Calibr8or Calibr8or_instance;

// App stubs
void Calibr8or_init() { Calibr8or_instance.BaseStart(); }

// Not using O_C Storage
size_t Calibr8or_storageSize() {return 0;}
size_t Calibr8or_save(void *storage) {return 0;}
size_t Calibr8or_restore(const void *storage) {return 0;}

void Calibr8or_isr() { return Calibr8or_instance.BaseController(); }

void Calibr8or_handleAppEvent(OC::AppEvent event) {
    if (event ==  OC::APP_EVENT_RESUME) {
        Calibr8or_instance.Resume();
    }
    if (event == OC::APP_EVENT_SUSPEND) {
        Calibr8or_instance.OnSendSysEx();
    }
}

void Calibr8or_loop() {} // Deprecated

void Calibr8or_menu() { Calibr8or_instance.BaseView(); }

void Calibr8or_screensaver() {
    // TODO: Consider a view like Quantermain
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
