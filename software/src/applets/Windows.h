#include <cstdint>
#include <math.h>   // for sqrtf()
#include "../HSWindowsExpLinker.h"

#define HEM_WINDOW_MAX_VALUE 255

// Enumerate the logic signals
enum LogicOp {
    LOGIC_A,
    LOGIC_B,
    LOGIC_AND,
    LOGIC_OR,
    LOGIC_XOR,
    LOGIC_FF,
    LOGIC_COUNT // number of ops
};

// Where CV2 can be routed:
enum CV2Target {
    CV2_OFF,
    CV2_A_SHIFT,
    CV2_A_SIZE,
    CV2_B_SHIFT,
    CV2_B_SIZE,
    CV2_B_IN,   // <--- NEW OPTION: Window B uses In(1) as main input
    CV2_COUNT
};

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
        // Window A base
        shiftA_base_      = 128;  // Center
        sizeA_base_       = 32;   // Half-width
        in_windowA_       = false;

        // Window B base
        shiftB_base_      = 64;
        sizeB_base_       = 16;
        in_windowB_       = false;

        // Logic 
        logic_and_        = false;
        logic_or_         = false;
        logic_xor_        = false;
        ff_state_         = false;
        last_any_         = false;

        // Default Gate assignments
        gate0_mode_       = LOGIC_A;
        gate1_mode_       = LOGIC_B;

        // CV2 routing
        cv2_target_       = CV2_OFF;

        // Param Navigation
        param_index_      = 0;  // which param is selected
        editing_          = false;
    }

    void Controller() {
        // ----------------------------
        // 1) Decide Window B's input
        //    If CV2_B_IN is selected, use In(1) for B; else In(0)
        // ----------------------------
        int inputA = In(0);
        int inputB;
        if (cv2_target_ == CV2_B_IN) {
            inputB = In(1);
        } else {
            inputB = In(0);
        }

        // ----------------------------
        // 2) Apply CV2 if assigned (unless B.In is used for B)
        // ----------------------------
        int cv2_raw = In(1);                            // 0..16383
        int cv2_val = Proportion(cv2_raw, HEMISPHERE_MAX_CV, 255); // 0..255

        // Start final SHIFT/SIZE from base
        shiftA_ = shiftA_base_;
        sizeA_  = sizeA_base_;
        shiftB_ = shiftB_base_;
        sizeB_  = sizeB_base_;

        // If we've chosen B.In, skip SHIFT/SIZE offset for B
        switch (cv2_target_) {
            case CV2_A_SHIFT:
                shiftA_ = shiftA_base_ + cv2_val;
                if (shiftA_ > 255) shiftA_ = 255;
                break;
            case CV2_A_SIZE:
                sizeA_ = sizeA_base_ + cv2_val;
                if (sizeA_ > 127) sizeA_ = 127;
                break;
            case CV2_B_SHIFT:
                // Only apply if NOT in B.In mode
                // (But we already handle that by controlling the flow here)
                shiftB_ = shiftB_base_ + cv2_val;
                if (shiftB_ > 255) shiftB_ = 255;
                break;
            case CV2_B_SIZE:
                sizeB_ = sizeB_base_ + cv2_val;
                if (sizeB_ > 127) sizeB_ = 127;
                break;
            default: // CV2_OFF or CV2_B_IN
                break;
        }

        // ----------------------------
        // 3) Window A logic
        // ----------------------------
        int lowerA = Proportion(shiftA_ - sizeA_, 255, HEMISPHERE_MAX_CV);
        int upperA = Proportion(shiftA_ + sizeA_, 255, HEMISPHERE_MAX_CV);
        if (lowerA < 0) lowerA = 0;
        in_windowA_ = (inputA >= lowerA && inputA <= upperA);

        // ----------------------------
        // 4) Window B logic
        // ----------------------------
        int lowerB = Proportion(shiftB_ - sizeB_, 255, HEMISPHERE_MAX_CV);
        int upperB = Proportion(shiftB_ + sizeB_, 255, HEMISPHERE_MAX_CV);
        if (lowerB < 0) lowerB = 0;
        in_windowB_ = (inputB >= lowerB && inputB <= upperB);

        // ----------------------------
        // Basic logic + flip-flop
        // ----------------------------
        logic_and_ = in_windowA_ && in_windowB_;
        logic_or_  = in_windowA_ || in_windowB_;
        logic_xor_ = in_windowA_ ^  in_windowB_;

        bool any_now = (in_windowA_ || in_windowB_);
        if (any_now && !last_any_) {
            ff_state_ = !ff_state_;
        }
        last_any_ = any_now;

        // Once you compute in_windowA_ etc., do:
            WindowsLinker::win_a   = in_windowA_;
            WindowsLinker::win_and = logic_and_;
            WindowsLinker::win_or  = logic_or_;
            WindowsLinker::win_xor = logic_xor_;
            WindowsLinker::win_ff  = ff_state_;
            WindowsLinker::win_b   = in_windowB_;


        // Decide GateOut(0) & GateOut(1)
        bool gate0 = EvaluateLogic(gate0_mode_);
        bool gate1 = EvaluateLogic(gate1_mode_);
        GateOut(0, gate0);
        GateOut(1, gate1);
    }

    // --------------------------------------------------
    // View
    // --------------------------------------------------
    void View() {
        // Fill(0); // optional

        // ----------------------------
        // Window A bar
        // ----------------------------
        gfxFrame(1, 10, 62, 6);
        int bar_leftA  = Proportion(shiftA_ - sizeA_, 255, 62);
        int bar_rightA = Proportion(shiftA_ + sizeA_, 255, 62);
        if (bar_leftA < 0)  bar_leftA = 0;
        if (bar_rightA> 62) bar_rightA= 62;
        if (bar_leftA < bar_rightA) {
            gfxRect(1 + bar_leftA, 10, bar_rightA - bar_leftA, 6);
        }
        // Center line
        int shiftA_x = Proportion(shiftA_, 255, 62);
        gfxLine(1 + shiftA_x, 10, 1 + shiftA_x, 16);

        // Blink if param_index in [0..1]
        if (!editing_ && param_index_ < 2) {
            bool blink = (CursorBlink() & 0x80);
            if (blink) gfxFrame(1, 10, 62, 6);
        }

        // ----------------------------
        // Window B bar
        // ----------------------------
        gfxFrame(1, 20, 62, 6);
        int bar_leftB  = Proportion(shiftB_ - sizeB_, 255, 62);
        int bar_rightB = Proportion(shiftB_ + sizeB_, 255, 62);
        if (bar_leftB < 0)   bar_leftB = 0;
        if (bar_rightB> 62)  bar_rightB= 62;
        if (bar_leftB < bar_rightB) {
            gfxRect(1 + bar_leftB, 20, bar_rightB - bar_leftB, 6);
        }
        int shiftB_x = Proportion(shiftB_, 255, 62);
        gfxLine(1 + shiftB_x, 20, 1 + shiftB_x, 26);

        // Blink if param_index in [2..3]
        if (!editing_ && (param_index_ >=2 && param_index_ <4)) {
            bool blink = (CursorBlink() & 0x80);
            if (blink) gfxFrame(1, 20, 62, 6);
        }

        // ----------------------------
        // Single-pixel line at y=15 for In(0)
        // ----------------------------
        int cv_val0 = In(0);
        int cv_x0   = Proportion(cv_val0, HEMISPHERE_MAX_CV, 62);
        gfxLine(1, 17, 1 + cv_x0, 17);

        // ----------------------------
        // Single-pixel line at y=28 for In(0)
        // ----------------------------
        int cv_val1 = In(1);
        int cv_x1   = Proportion(cv_val1, HEMISPHERE_MAX_CV, 62);
        gfxLine(1, 27, 1 + cv_x1, 27);

        // ----------------------------
        // Logic circles & labels
        // ----------------------------
        gfxPos(5  - 3, 35); gfxPrint("A");
        gfxPos(15 - 3, 35); gfxPrint("&");
        gfxPos(25 - 3, 35); gfxPrint("O");
        gfxPos(35 - 3, 35); gfxPrint("X");
        gfxPos(45 - 3, 35); gfxPrint("F");
        gfxPos(55 - 3, 35); gfxPrint("B");

        DrawCircleFillOrFrame(5,  45, in_windowA_);
        DrawCircleFillOrFrame(15, 45, logic_and_);
        DrawCircleFillOrFrame(25, 45, logic_or_);
        DrawCircleFillOrFrame(35, 45, logic_xor_);
        DrawCircleFillOrFrame(45, 45, ff_state_);
        DrawCircleFillOrFrame(55, 45, in_windowB_);

        // ----------------------------
        // Param text at y=56
        // ----------------------------
        gfxPos(0, 56);
        gfxPrint(paramLabel());
        gfxPrint(": ");
        gfxPrint(paramValueString());
        if (editing_) gfxPrint("*");
    }

    // --------------------------------------------------
    // Button & Encoder
    // --------------------------------------------------
    void OnButtonPress() {
        editing_ = !editing_;
    }

    void OnEncoderMove(int direction) {
        if (!editing_) {
            // Navigate among [0..6]
            param_index_ += direction;
            if (param_index_ < 0)   param_index_ = 6;
            if (param_index_ > 6)   param_index_ = 0;
        } else {
            // Edit the selected param
            switch (param_index_) {
                case 0: // SHIFT A
                    shiftA_base_ = constrain(shiftA_base_ + direction, 0, 255);
                    break;
                case 1: // SIZE A
                    sizeA_base_ = constrain(sizeA_base_ + direction, 0, 127);
                    break;
                case 2: // SHIFT B
                    shiftB_base_ = constrain(shiftB_base_ + direction, 0, 255);
                    break;
                case 3: // SIZE B
                    sizeB_base_ = constrain(sizeB_base_ + direction, 0, 127);
                    break;
                case 4: // Gate0 operator
                {
                    int nm = (int)gate0_mode_ + direction;
                    if (nm < 0) nm = LOGIC_COUNT - 1;
                    if (nm >= LOGIC_COUNT) nm = 0;
                    gate0_mode_ = (LogicOp)nm;
                    break;
                }
                case 5: // Gate1 operator
                {
                    int nm = (int)gate1_mode_ + direction;
                    if (nm < 0) nm = LOGIC_COUNT - 1;
                    if (nm >= LOGIC_COUNT) nm = 0;
                    gate1_mode_ = (LogicOp)nm;
                    break;
                }
                case 6: // CV2 routing
                {
                    int new_t = (int)cv2_target_ + direction;
                    if (new_t < 0) new_t = CV2_COUNT -1;
                    if (new_t >= (int)CV2_COUNT) new_t = 0;
                    cv2_target_ = (CV2Target)new_t;
                    break;
                }
            }
        }
    }

    // --------------------------------------------------
    // Data Persistence
    // --------------------------------------------------
    uint64_t OnDataRequest() {
        uint64_t data = 0;
        // SHIFT A, SIZE A
        Pack(data, PackLocation {0, 8}, shiftA_base_);
        Pack(data, PackLocation {8, 7}, sizeA_base_);
        // SHIFT B, SIZE B
        Pack(data, PackLocation {15,8}, shiftB_base_);
        Pack(data, PackLocation {23,7}, sizeB_base_);
        // Gate ops
        Pack(data, PackLocation {30,4}, gate0_mode_);
        Pack(data, PackLocation {34,4}, gate1_mode_);
        // cv2_target_ in 3 bits => 0..6 (we only use 0..5, but it fits)
        Pack(data, PackLocation {38,3}, cv2_target_);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        shiftA_base_ = Unpack(data, PackLocation {0,8});
        sizeA_base_  = Unpack(data, PackLocation {8,7});
        shiftB_base_ = Unpack(data, PackLocation {15,8});
        sizeB_base_  = Unpack(data, PackLocation {23,7});
        gate0_mode_  = (LogicOp)Unpack(data, PackLocation {30,4});
        gate1_mode_  = (LogicOp)Unpack(data, PackLocation {34,4});
        cv2_target_  = (CV2Target)Unpack(data, PackLocation {38,3});
    }

protected:
    void SetHelp() {
        help[HELP_DIGITAL1] = "Gate0 / logic";
        help[HELP_DIGITAL2] = "Gate1 / logic";
        help[HELP_CV1]      = "A Input(0)";
        help[HELP_CV2]      = "Assigned param or B.In";
        help[HELP_OUT1]     = "Selected Op";
        help[HELP_OUT2]     = "Selected Op";
        help[HELP_EXTRA1]   = "";
        help[HELP_EXTRA2]   = "";
    }

private:
    // ---------------------------
    // Window A
    // ---------------------------
    int  shiftA_base_;
    int  sizeA_base_;
    int  shiftA_;  
    int  sizeA_;
    bool in_windowA_;

    // ---------------------------
    // Window B
    // ---------------------------
    int  shiftB_base_;
    int  sizeB_base_;
    int  shiftB_;
    int  sizeB_;
    bool in_windowB_;

    // ---------------------------
    // Logic
    // ---------------------------
    bool logic_and_;
    bool logic_or_;
    bool logic_xor_;
    bool ff_state_;
    bool last_any_;

    LogicOp gate0_mode_;
    LogicOp gate1_mode_;

    // ---------------------------
    // CV2 routing
    // ---------------------------
    CV2Target cv2_target_;

    // Navigation
    int  param_index_;
    bool editing_;

    bool EvaluateLogic(LogicOp mode) {
        switch (mode) {
            case LOGIC_A:   return in_windowA_;
            case LOGIC_B:   return in_windowB_;
            case LOGIC_AND: return logic_and_;
            case LOGIC_OR:  return logic_or_;
            case LOGIC_XOR: return logic_xor_;
            case LOGIC_FF:  return ff_state_;
            default:        return false;
        }
    }

    const char* paramLabel() {
        switch (param_index_) {
            case 0: return "A.Sh";
            case 1: return "A.Sz";
            case 2: return "B.Sh";
            case 3: return "B.Sz";
            case 4: return "G0";
            case 5: return "G1";
            case 6: return "CV2";
        }
        return "?";
    }

    char* paramValueString() {
        static char buf[8];
        switch (param_index_) {
            case 0: snprintf(buf, sizeof(buf), "%d", shiftA_base_); return buf;
            case 1: snprintf(buf, sizeof(buf), "%d", sizeA_base_ ); return buf;
            case 2: snprintf(buf, sizeof(buf), "%d", shiftB_base_); return buf;
            case 3: snprintf(buf, sizeof(buf), "%d", sizeB_base_ ); return buf;
            case 4: return logicOpName(gate0_mode_);
            case 5: return logicOpName(gate1_mode_);
            case 6: return cv2TargetName(cv2_target_);
        }
        return (char*)"";
    }

    char* logicOpName(LogicOp op) {
        switch (op) {
            case LOGIC_A:   return (char*)"A";
            case LOGIC_B:   return (char*)"B";
            case LOGIC_AND: return (char*)"&";
            case LOGIC_OR:  return (char*)"O";
            case LOGIC_XOR: return (char*)"X";
            case LOGIC_FF:  return (char*)"F";
            default:        return (char*)"?";
        }
    }

    // CV2_B_IN => "B.In"
    char* cv2TargetName(CV2Target t) {
        switch (t) {
            case CV2_OFF:     return (char*)"Off";
            case CV2_A_SHIFT: return (char*)"A.Sh";
            case CV2_A_SIZE:  return (char*)"A.Sz";
            case CV2_B_SHIFT: return (char*)"B.Sh";
            case CV2_B_SIZE:  return (char*)"B.Sz";
            case CV2_B_IN:    return (char*)"B.In";
            default:          return (char*)"?";
        }
    }

    int clamp(int v, int lo, int hi) {
        if (v < lo) return lo;
        if (v > hi) return hi;
        return v;
    }

    void drawFilledCircle(int cx, int cy, int r) {
        for (int dy = -r; dy <= r; dy++) {
            int dx = (int)sqrtf((float)(r*r - dy*dy));
            gfxLine(cx - dx, cy + dy, cx + dx, cy + dy);
        }
    }

    void DrawCircleFillOrFrame(int x, int y, bool state) {
        int r = 3;
        if (state) drawFilledCircle(x, y, r);
        else       gfxCircle(x, y, r);
    }
};
