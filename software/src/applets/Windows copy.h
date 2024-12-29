#include <cstdint>
#include <math.h>   // for sqrt() in our custom fill function

#define HEM_WINDOW_MAX_VALUE 255

class Windows : public HemisphereApplet {
public:
    const char* applet_name() {
        return "Windows";
    }
    const uint8_t* applet_icon() {
        return nullptr;
    }

    // --------------------------------------------------
    // Lifecycle
    // --------------------------------------------------
    void Start() {
        // Window A defaults
        shiftA_      = 128;  // Center
        sizeA_       = 32;   // Half-width
        in_windowA_  = false;
        edit_shiftA_ = true; // We'll edit SHIFT/size on Window A

        // Window B defaults (hard-coded for example)
        shiftB_      = 64;   
        sizeB_       = 16;
        in_windowB_  = false;

        // Logic FF initialization
        ff_state_   = false;  
        last_any_   = false;  // tracks the old state of (A||B)
    }

    void Controller() {
        // 1) Read the same input for both windows
        int input_val = In(0);

        // 2) Window A logic
        shiftA_ = constrain(shiftA_, 0, HEM_WINDOW_MAX_VALUE);
        sizeA_  = constrain(sizeA_,  0, HEM_WINDOW_MAX_VALUE / 2);

        int lowerA = Proportion(shiftA_ - sizeA_, HEM_WINDOW_MAX_VALUE, HEMISPHERE_MAX_CV);
        int upperA = Proportion(shiftA_ + sizeA_, HEM_WINDOW_MAX_VALUE, HEMISPHERE_MAX_CV);
        in_windowA_ = (input_val >= lowerA && input_val <= upperA);

        // 3) Window B logic
        shiftB_ = constrain(shiftB_, 0, HEM_WINDOW_MAX_VALUE);
        sizeB_  = constrain(sizeB_,  0, HEM_WINDOW_MAX_VALUE / 2);

        int lowerB = Proportion(shiftB_ - sizeB_, HEM_WINDOW_MAX_VALUE, HEMISPHERE_MAX_CV);
        int upperB = Proportion(shiftB_ + sizeB_, HEM_WINDOW_MAX_VALUE, HEMISPHERE_MAX_CV);
        in_windowB_ = (input_val >= lowerB && input_val <= upperB);

        // Gate outputs for Windows
        GateOut(0, in_windowA_);
        GateOut(1, in_windowB_);

        // --------------------------------------------------
        // Additional Logic
        // --------------------------------------------------
        logic_and_ = in_windowA_ && in_windowB_;
        logic_or_  = in_windowA_ || in_windowB_;
        logic_xor_ = in_windowA_ ^  in_windowB_;

        // Flip-Flop toggles on the rising edge of (A||B)
        bool any_now = (in_windowA_ || in_windowB_);
        if (any_now && !last_any_) {
            // rising edge => toggle FF state
            ff_state_ = !ff_state_;
        }
        last_any_ = any_now;
    }

    // --------------------------------------------------
    // View
    // --------------------------------------------------
    void View() {
    // Optional: clear the screen
    // Fill(0);

    // ----------------------------
    // Window A bar (y=10)
    // ----------------------------
    gfxFrame(1, 10, 62, 6);
    int bar_leftA  = Proportion(shiftA_ - sizeA_, HEM_WINDOW_MAX_VALUE, 62);
    int bar_rightA = Proportion(shiftA_ + sizeA_, HEM_WINDOW_MAX_VALUE, 62);
    bar_leftA  = clamp(bar_leftA,  0, 62);
    bar_rightA = clamp(bar_rightA, 0, 62);
    if (bar_leftA < bar_rightA) {
        gfxRect(1 + bar_leftA, 10, bar_rightA - bar_leftA, 6);
    }
    // Center line A
    int shiftA_x = Proportion(shiftA_, HEM_WINDOW_MAX_VALUE, 62);
    gfxLine(1 + shiftA_x, 10, 1 + shiftA_x, 16);

    // Possibly blink the bar if SHIFT A is selected
    bool blink = (CursorBlink() & 0x80);
    if (blink && edit_shiftA_) {
        gfxFrame(1, 10, 62, 6);
    }

    // ----------------------------
    // Window B bar (y=20)
    // ----------------------------
    gfxFrame(1, 20, 62, 6);
    int bar_leftB  = Proportion(shiftB_ - sizeB_, HEM_WINDOW_MAX_VALUE, 62);
    int bar_rightB = Proportion(shiftB_ + sizeB_, HEM_WINDOW_MAX_VALUE, 62);
    bar_leftB  = clamp(bar_leftB,  0, 62);
    bar_rightB = clamp(bar_rightB, 0, 62);
    if (bar_leftB < bar_rightB) {
        gfxRect(1 + bar_leftB, 20, bar_rightB - bar_leftB, 6);
    }
    // Center line B
    int shiftB_x = Proportion(shiftB_, HEM_WINDOW_MAX_VALUE, 62);
    gfxLine(1 + shiftB_x, 20, 1 + shiftB_x, 26);

    // ----------------------------
    // Single-pixel line at y=30 for In(0)
    // ----------------------------
    int cv_val = In(0);
    int cv_x   = Proportion(cv_val, HEMISPHERE_MAX_CV, 62);
    gfxLine(1, 30, 1 + cv_x, 30);

    // ----------------------------
    // Circles for A, AND, OR, XOR, FF, B at y=45
    // and centered labels at y=35
    // x coords: 5,15,25,35,45,55
    // offset text by 3 px left so it's centered on each circle
    // ----------------------------
    gfxPos(5  - 3, 35); gfxPrint("A");
    gfxPos(15 - 3, 35); gfxPrint("A");
    gfxPos(25 - 3, 35); gfxPrint("O");
    gfxPos(35 - 3, 35); gfxPrint("X");
    gfxPos(45 - 3, 35); gfxPrint("F");
    gfxPos(55 - 3, 35); gfxPrint("B");

    DrawCircleFillOrFrame(5,  45, in_windowA_); // A
    DrawCircleFillOrFrame(15, 45, logic_and_);  // AND
    DrawCircleFillOrFrame(25, 45, logic_or_);   // OR
    DrawCircleFillOrFrame(35, 45, logic_xor_);  // XOR
    DrawCircleFillOrFrame(45, 45, ff_state_);   // FF
    DrawCircleFillOrFrame(55, 45, in_windowB_); // B
}


    // --------------------------------------------------
    // Button & Encoder (only controlling Window A)
    // --------------------------------------------------
    void OnButtonPress() {
        // Toggle SHIFT vs SIZE for Window A
        edit_shiftA_ = !edit_shiftA_;
    }

    void OnEncoderMove(int direction) {
        if (edit_shiftA_) {
            shiftA_ = constrain(shiftA_ + direction, 0, HEM_WINDOW_MAX_VALUE);
        } else {
            sizeA_  = constrain(sizeA_ + direction, 0, HEM_WINDOW_MAX_VALUE / 2);
        }
    }

    // --------------------------------------------------
    // Data Persistence
    // --------------------------------------------------
    uint64_t OnDataRequest() {
        uint64_t data = 0;
        // Window A SHIFT/SIZE
        Pack(data, PackLocation {0,8}, shiftA_);
        Pack(data, PackLocation {8,8}, sizeA_);
        // Window B SHIFT/SIZE
        Pack(data, PackLocation {16,8}, shiftB_);
        Pack(data, PackLocation {24,8}, sizeB_);
        // You might also store ff_state_ or last_any_ if you want them persisted
        return data;
    }

    void OnDataReceive(uint64_t data) {
        shiftA_ = Unpack(data, PackLocation {0,8});
        sizeA_  = Unpack(data, PackLocation {8,8});
        shiftB_ = Unpack(data, PackLocation {16,8});
        sizeB_  = Unpack(data, PackLocation {24,8});
    }

protected:
    void SetHelp() {
        help[HELP_DIGITAL1] = "Gate A";
        help[HELP_DIGITAL2] = "Gate B";
        help[HELP_CV1]      = "Signal In(0)";
        help[HELP_CV2]      = "";
        help[HELP_OUT1]     = "Win A Gate";
        help[HELP_OUT2]     = "Win B Gate";
        help[HELP_EXTRA1]   = "";
        help[HELP_EXTRA2]   = "";
    }

private:
    // -----------------------------
    // Window A
    // -----------------------------
    int  shiftA_;
    int  sizeA_;
    bool in_windowA_;
    bool edit_shiftA_;

    // -----------------------------
    // Window B
    // -----------------------------
    int  shiftB_;
    int  sizeB_;
    bool in_windowB_;

    // -----------------------------
    // Logic signals
    // -----------------------------
    bool logic_and_;
    bool logic_or_;
    bool logic_xor_;
    
    // Flip-flop
    bool ff_state_;
    bool last_any_;  // track old (A||B)

    // -----------------------------
    // Helper: clamp
    // -----------------------------
    int clamp(int v, int lo, int hi) {
        if (v < lo) return lo;
        if (v > hi) return hi;
        return v;
    }

    // -----------------------------
    // Helper: drawFilledCircle
    // -----------------------------
    // A naive scanline approach to fill a circle
    void drawFilledCircle(int cx, int cy, int r) {
        for (int dy = -r; dy <= r; dy++) {
            int dx = (int)sqrtf((float)(r*r - dy*dy));
            gfxLine(cx - dx, cy + dy, cx + dx, cy + dy);
        }
    }

    // -----------------------------
    // Helper: Draw a small circle
    // (filled if state=true, outline if false)
    // -----------------------------
    void DrawCircleFillOrFrame(int x, int y, bool state) {
        int r = 3;
        if (state) {
            drawFilledCircle(x, y, r);
        } else {
            gfxCircle(x, y, r);
        }
    }
};
