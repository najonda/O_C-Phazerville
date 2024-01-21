// Copyright (c) 2018, Jason Justian
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

class ClockDivider : public HemisphereApplet {
public:

    static constexpr int CLOCKDIV_MAX = 32;

    const char* applet_name() {
        return "Clock Div";
    }

    void Start() { }

    void Controller() {
        uint32_t this_tick = OC::CORE::ticks;

        // Modulate setting via CV
        // Set division via CV
        ForEachChannel(ch)
        {
          div_m[ch] = div[ch];
          Modulate(div_m[ch], ch, -CLOCKDIV_MAX, CLOCKDIV_MAX);
        }

        if (Clock(1)) { // Reset
            ForEachChannel(ch) count[ch] = 0;
        }

        // The input was clocked; set timing info
        if (Clock(0)) {
        		cycle_time = ClockCycleTicks(0);
            // At the clock input, handle clock division
            ForEachChannel(ch)
            {
                if (div_m[ch] > 0) { // Positive value indicates clock division
                    count[ch]++;
                    if (count[ch] == 1) ClockOut(ch); // fire on first step
                    if (count[ch] >= div_m[ch]) count[ch] = 0; // Reset on last step
                }
                // if (div_m[ch] == 0) doNothing;
                if (div_m[ch] < 0) {
                    // Calculate next clock for multiplication on each clock
                    int tick_interval = (cycle_time / -div_m[ch]);
                    next_clock[ch] = this_tick + tick_interval;
                    count[ch] = 1;
                    ClockOut(ch); // Sync
                }
            }
        }

        // Handle clock multiplication
        ForEachChannel(ch)
        {
            if (div_m[ch] < 0) { // Negative value indicates clock multiplication
                if (count[ch] < -div_m[ch] && this_tick >= next_clock[ch]) {
                    int tick_interval = (cycle_time / -div_m[ch]);
                    next_clock[ch] += tick_interval;
                    count[ch]++;
                    ClockOut(ch);
                }
            }
        }
    }

    void View() {
        DrawSelector();
    }

    void OnButtonPress() {
        CursorAction(cursor, 1);
    }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, 1);
            return;
        }

        div[cursor] += direction;
        CONSTRAIN(div[cursor], -CLOCKDIV_MAX, CLOCKDIV_MAX);

        count[cursor] = 0; // Start the count over so things aren't missed
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,8}, div[0] + 32);
        Pack(data, PackLocation {8,8}, div[1] + 32);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        div[0] = Unpack(data, PackLocation {0,8}) - 32;
        div[1] = Unpack(data, PackLocation {8,8}) - 32;
    }

protected:
    void SetHelp() {
        help[HEMISPHERE_HELP_DIGITALS] = "1=Clock 2=Reset";
        help[HEMISPHERE_HELP_CVS] = "Div/Mult Ch1,Ch2";
        help[HEMISPHERE_HELP_OUTS] = "Clk A=Ch1 B=Ch2";
        help[HEMISPHERE_HELP_ENCODER] = "Div,Mult";
    }

private:
    int div[2] = {1, 2}; // Division setting. Positive for divisions, negative for multipliers. Zero is mute.
    int div_m[2]; // CV modulated value
    int count[2] = {0,0}; // Number of clocks since last output (for clock divide)
    uint32_t next_clock[2] = {0,0}; // Tick number for the next output (for clock multiply)
    int cursor = 0; // Which output is currently being edited
    int cycle_time = 0; // Cycle time between the last two clock inputs

    void DrawSelector() {
        static const char * chan_name[] = { "A", "B", "C", "D" };
        ForEachChannel(ch)
        {
            int y = 15 + (ch * 25);

            gfxPrint(1, y, chan_name[ch+hemisphere*2]);
            if (div_m[ch] > 0) {
                gfxPrint(" /");
                gfxPrint(div_m[ch]);
                gfxPrint(" Div");
            }
            if (div_m[ch] < 0) {
                gfxPrint(" x");
                gfxPrint(-div_m[ch]);
                gfxPrint(" Mult");
            }
            if (div_m[ch] == 0) {
                gfxPrint(" (off)");
            }

            if (div_m[ch] != div[ch])
              gfxIcon(18, y+8, CV_ICON);
        }
        gfxCursor(12, 23 + (cursor * 25), 50);
    }
};


////////////////////////////////////////////////////////////////////////////////
//// Hemisphere Applet Functions
///
///  Once you run the find-and-replace to make these refer to ClockDivider,
///  it's usually not necessary to do anything with these functions. You
///  should prefer to handle things in the HemisphereApplet child class
///  above.
////////////////////////////////////////////////////////////////////////////////
ClockDivider ClockDivider_instance[2];

void ClockDivider_Start(bool hemisphere) {
    ClockDivider_instance[hemisphere].BaseStart(hemisphere);
}

void ClockDivider_Controller(bool hemisphere, bool forwarding) {
    ClockDivider_instance[hemisphere].BaseController(forwarding);
}

void ClockDivider_View(bool hemisphere) {
    ClockDivider_instance[hemisphere].BaseView();
}

void ClockDivider_OnButtonPress(bool hemisphere) {
    ClockDivider_instance[hemisphere].OnButtonPress();
}

void ClockDivider_OnEncoderMove(bool hemisphere, int direction) {
    ClockDivider_instance[hemisphere].OnEncoderMove(direction);
}

void ClockDivider_ToggleHelpScreen(bool hemisphere) {
    ClockDivider_instance[hemisphere].HelpScreen();
}

uint64_t ClockDivider_OnDataRequest(bool hemisphere) {
    return ClockDivider_instance[hemisphere].OnDataRequest();
}

void ClockDivider_OnDataReceive(bool hemisphere, uint64_t data) {
    ClockDivider_instance[hemisphere].OnDataReceive(data);
}
