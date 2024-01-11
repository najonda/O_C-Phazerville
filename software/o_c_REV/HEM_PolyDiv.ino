// Copyright (c) 2024, Nicholas Michalek
// Copyright (c) 2022, Benjamin Rosenbach
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

/* Used ProbDiv applet from benirose as a template */

#include "HSProbLoopLinker.h" // singleton for linking ProbDiv and ProbMelo

class PolyDiv : public HemisphereApplet {
public:

    static constexpr uint8_t MAX_DIV = 63;
    static constexpr uint8_t NUM_STEPS = 5;

    enum PolyDivCursor {
        STEP1A, STEP2A, STEP3A, STEP4A, STEP5A,
        STEP1B, STEP2B, STEP3B, STEP4B, STEP5B,
        LAST_SETTING = STEP5B
    };

    struct DivSequence {
      int step_index;
      int clock_count;
      // duration as number of clock pulses
      uint8_t steps[NUM_STEPS];

      bool Poke() {
        // reset case
        if (step_index < 0) {
            step_index = 0;
            return steps[step_index] > 0;
        }

        // quota achieved, advance to next enabled step
        if (++clock_count >= steps[step_index]) {
            clock_count = 0;

            int i = 0;
            do {
                ++step_index %= NUM_STEPS;
                ++i;
            } while (steps[step_index] == 0 && i < NUM_STEPS);

            return steps[step_index] > 0;
        }
        return false;
      }
      void Reset() {
        step_index = -1;
        clock_count = 0;
      }

    } div_seq[2] = {
      { -1, 0, { 4, 0, 0, 0, 0 } },
      { -1, 0, { 8, 3, 3, 2, 0 } },
    };

    const char* applet_name() {
        return "PolyDiv";
    }

    void Start() {
    }

    void Reset() {
        ForEachChannel(ch) {
          div_seq[ch].Reset();
        }
    }
    void TrigOut(int ch) {
        ClockOut(ch);
        loop_linker->Trigger(ch);
        pulse_animation[ch] = HEMISPHERE_PULSE_ANIMATION_TIME;
    }

    void Controller() {
        loop_linker->RegisterDiv(hemisphere);

        // reset
        if (Clock(1)) {
            Reset();
        }

        if (Clock(0)) {
          // sequence advance, get trigger bits
          bool trig_q[2] = { div_seq[0].Poke(), div_seq[1].Poke() };

          ForEachChannel(ch) {
            // XOR with positive CV gate
            bool trig = (trig_q[ch] != (In(ch) > 6*128));

            // negative CV gate enables XOR with other channel
            if (In(ch) < -6*128)
                trig = (trig != trig_q[1-ch]);

            if (trig) TrigOut(ch);
          }
        }

        ForEachChannel(ch) {
          if (pulse_animation[ch] > 0) {
              pulse_animation[ch]--;
          }
        }
    }

    void View() {
        DrawInterface();
    }

    void OnButtonPress() {
        CursorAction(cursor, LAST_SETTING);
    }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, LAST_SETTING);
            return;
        }

        const int ch = cursor / NUM_STEPS;
        const int s = cursor % NUM_STEPS;
        const int div = div_seq[ch].steps[s] + direction;
        //if (div < 0) div = MAX_DIV;
        div_seq[ch].steps[s] = constrain(div, 0, MAX_DIV);
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        const size_t b = 6; // bitsize
        ForEachChannel(ch) {
          for (size_t i = 0; i < NUM_STEPS; i++) {
            const uint8_t val = div_seq[ch].steps[i];
            Pack(data, PackLocation {ch*NUM_STEPS*b + i*b, b}, val);
          }
        }
        return data;
    }

    void OnDataReceive(uint64_t data) {
        const size_t b = 6; // bitsize
        ForEachChannel(ch) {
            for (size_t i = 0; i < NUM_STEPS; i++) {
                div_seq[ch].steps[i] = Unpack(data, PackLocation {ch*NUM_STEPS*b + i*b, b});
                div_seq[ch].steps[i] = constrain(div_seq[ch].steps[i], 0, MAX_DIV);
            }
            // step 1 cannot be zero
            if (div_seq[ch].steps[0] == 0) ++div_seq[ch].steps[0];
        }
        Reset();
    }

protected:
    void SetHelp() {
        //                               "------------------" <-- Size Guide
        help[HEMISPHERE_HELP_DIGITALS] = "1=Clock  2=Reset";
        help[HEMISPHERE_HELP_CVS]      = "1=XOR    2=XOR";
        help[HEMISPHERE_HELP_OUTS]     = "A=Trig   B=Trig";
        help[HEMISPHERE_HELP_ENCODER]  = "Division per step";
        //                               "------------------" <-- Size Guide
    }
    
private:
    int cursor; // PolyDivCursor 

    int pulse_animation[2] = {0,0};

    ProbLoopLinker *loop_linker = loop_linker->get();

    void DrawInterface() {
      // divisions
      ForEachChannel(ch) {
        for(int i = 0; i < NUM_STEPS; i++) {
          if (div_seq[ch].steps[i] == 0 && cursor != i + ch*NUM_STEPS) continue;

          gfxPrint(1 + 31*ch, 15 + (i*10), div_seq[ch].steps[i]);
          DrawSlider(14 + 31*ch, 15 + (i*10), 14, div_seq[ch].steps[i], MAX_DIV, cursor == i+ch*NUM_STEPS);

          if (div_seq[ch].step_index == i)
            gfxIcon(28 + 31*ch, 15 + i*10, LEFT_BTN_ICON);
        }
        // flash division when triggered
        if (pulse_animation[ch] > 0 && div_seq[ch].step_index >= 0) {
          gfxInvert(1 + 31*ch, 15 + (div_seq[ch].step_index*10), 12, 8);
        }
      }
    }

};


////////////////////////////////////////////////////////////////////////////////
//// Hemisphere Applet Functions
///
///  Once you run the find-and-replace to make these refer to PolyDiv,
///  it's usually not necessary to do anything with these functions. You
///  should prefer to handle things in the HemisphereApplet child class
///  above.
////////////////////////////////////////////////////////////////////////////////
PolyDiv PolyDiv_instance[2];

void PolyDiv_Start(bool hemisphere) {PolyDiv_instance[hemisphere].BaseStart(hemisphere);}
void PolyDiv_Controller(bool hemisphere, bool forwarding) {PolyDiv_instance[hemisphere].BaseController(forwarding);}
void PolyDiv_View(bool hemisphere) {PolyDiv_instance[hemisphere].BaseView();}
void PolyDiv_OnButtonPress(bool hemisphere) {PolyDiv_instance[hemisphere].OnButtonPress();}
void PolyDiv_OnEncoderMove(bool hemisphere, int direction) {PolyDiv_instance[hemisphere].OnEncoderMove(direction);}
void PolyDiv_ToggleHelpScreen(bool hemisphere) {PolyDiv_instance[hemisphere].HelpScreen();}
uint64_t PolyDiv_OnDataRequest(bool hemisphere) {return PolyDiv_instance[hemisphere].OnDataRequest();}
void PolyDiv_OnDataReceive(bool hemisphere, uint64_t data) {PolyDiv_instance[hemisphere].OnDataReceive(data);}
