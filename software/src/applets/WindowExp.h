#include <cstdint>
#include "HSWindowsExpLinker.h"  // Access the 6 booleans from Windows

// 6 logic ops for GateOut(2) and GateOut(3)
enum WindowExpLogicOp {
    WLOGIC_A,
    WLOGIC_AND,
    WLOGIC_OR,
    WLOGIC_XOR,
    WLOGIC_FF,
    WLOGIC_B,
    WLOGIC_COUNT
};

class WindowExp : public HemisphereApplet {
public:
    // --------------------------------------------------
    // Applet Metadata
    // --------------------------------------------------
    const char* applet_name() override { return "WindowExp"; }
    const uint8_t* applet_icon() override { return nullptr; }

    // --------------------------------------------------
    // Lifecycle
    // --------------------------------------------------
    void Start() override {
        gate_mode_2_ = WLOGIC_A;   // default for GateOut(2)
        gate_mode_3_ = WLOGIC_B;   // default for GateOut(3)
        selected_gate_ = 0;        // 0 => G2 selected, 1 => G3 selected
        editing_ = false;
    }

    // --------------------------------------------------
    // Main logic
    // --------------------------------------------------
    void Controller() override {
        // 1) Fetch the 6 booleans from Windows
        bool a   = WindowsLinker::win_a;
        bool an  = WindowsLinker::win_and;
        bool o   = WindowsLinker::win_or;
        bool x   = WindowsLinker::win_xor;
        bool ff  = WindowsLinker::win_ff;
        bool b   = WindowsLinker::win_b;

        // 2) Evaluate GateOut(2) and GateOut(3)
        bool g2 = pickLogic(a, an, o, x, ff, b, gate_mode_2_);
        bool g3 = pickLogic(a, an, o, x, ff, b, gate_mode_3_);

        // 3) Output them
        GateOut(2, g2);
        GateOut(3, g3);

        // Store them for the UI so we can draw circles
        gate2_state_ = g2;
        gate3_state_ = g3;
    }

    // --------------------------------------------------
    // Drawing / UI
    // --------------------------------------------------
    void View() override {
        // Clear if you want: Fill(0);

        // 1) Show text labels for G2 / G3 logic operators
        // We'll place them near the top
        // x=10 => G2, x=50 => G3, y=0 for text
        gfxPos(7, 35);
        gfxPrint(logicOpName(gate_mode_2_));
        gfxPos(47, 35);
        gfxPrint(logicOpName(gate_mode_3_));

        // 2) Draw circles at the bottom for each gate's current state
        // e.g. y=45 for circles
        // If the circle is "filled" => gate is high; else outline
        // x=10 => Gate2, x=50 => Gate3
        DrawCircleFillOrFrame(10, 45, gate2_state_);
        DrawCircleFillOrFrame(50, 45, gate3_state_);

        // 3) Indicate which gate is selected. For instance, if selected_gate_ == 0, we blink around G2
        if (!editing_) {
            // Blink a small frame around the label if not in editing mode
            bool blink = (CursorBlink() & 0x80);
            if (blink) {
                if (selected_gate_ == 0) {
                    // a small frame around the G2 text at x=10,y=0
                    gfxFrame(5, 35, 20, 8);
                } else {
                    // a small frame around the G3 text at x=50,y=0
                    gfxFrame(45, 35, 20, 8);
                }
            }
        } else {
            // If editing, let's draw a small asterisk near the label
            if (selected_gate_ == 0) {
                gfxPos(13, 35);
                gfxPrint("*");
            } else {
                gfxPos(53, 35);
                gfxPrint("*");
            }
        }
    }

    // --------------------------------------------------
    // Button & Encoder
    // --------------------------------------------------
    void OnButtonPress() override {
        // Toggle editing
        editing_ = !editing_;
    }

    void OnEncoderMove(int direction) override {
        if (!editing_) {
            // Move selection between G2(0) and G3(1)
            selected_gate_ += direction;
            if (selected_gate_ < 0) selected_gate_ = 1;
            if (selected_gate_ > 1) selected_gate_ = 0;
        } else {
            // We are editing the operator for the currently selected gate
            if (selected_gate_ == 0) {
                // editing gate_mode_2_
                gate_mode_2_ = (WindowExpLogicOp)((int)gate_mode_2_ + direction);
                wrapGateMode(gate_mode_2_);
            } else {
                // editing gate_mode_3_
                gate_mode_3_ = (WindowExpLogicOp)((int)gate_mode_3_ + direction);
                wrapGateMode(gate_mode_3_);
            }
        }
    }

    // --------------------------------------------------
    // Data Persistence
    // --------------------------------------------------
    uint64_t OnDataRequest() override {
        uint64_t data = 0;
        // We’ll store gate2_mode in bits 0..3, gate3_mode in bits 4..7 
        Pack(data, PackLocation{0,4}, gate_mode_2_);
        Pack(data, PackLocation{4,4}, gate_mode_3_);
        return data;
    }

    void OnDataReceive(uint64_t data) override {
        gate_mode_2_ = (WindowExpLogicOp)Unpack(data, PackLocation{0,4});
        gate_mode_3_ = (WindowExpLogicOp)Unpack(data, PackLocation{4,4});
    }

protected:
    void SetHelp() override {
        help[HELP_DIGITAL1] = "Gate2 Op";
        help[HELP_DIGITAL2] = "Gate3 Op";
        help[HELP_CV1]      = "";
        help[HELP_CV2]      = "";
        help[HELP_OUT1]     = "GateOut2";
        help[HELP_OUT2]     = "GateOut3";
    }

private:
    // --------------------------------------------------
    // Internal logic
    // --------------------------------------------------
    // The currently-chosen operator for Gate2 / Gate3
    WindowExpLogicOp gate_mode_2_, gate_mode_3_;

    // Which gate is selected in the UI? 0 => Gate2, 1 => Gate3
    int selected_gate_;
    bool editing_;

    // Current gate states for drawing circles
    bool gate2_state_ = false;
    bool gate3_state_ = false;

    // Picks the correct boolean from the 6 booleans
    bool pickLogic(bool a, bool an, bool o, bool x, bool ff, bool b, WindowExpLogicOp mode) {
        switch (mode) {
            case WLOGIC_A:   return a;
            case WLOGIC_AND: return an;
            case WLOGIC_OR:  return o;
            case WLOGIC_XOR: return x;
            case WLOGIC_FF:  return ff;
            case WLOGIC_B:   return b;
            default:         return false;
        }
    }

    // Wrap the logic operator
    void wrapGateMode(WindowExpLogicOp &m) {
        if (m < 0) m = (WindowExpLogicOp)(WLOGIC_COUNT - 1);
        if (m >= WLOGIC_COUNT) m = (WindowExpLogicOp)0;
    }

    // Convert operator to text
    const char* logicOpName(WindowExpLogicOp op) {
        switch (op) {
            case WLOGIC_A:   return "A";
            case WLOGIC_AND: return "AND";
            case WLOGIC_OR:  return "OR";
            case WLOGIC_XOR: return "XOR";
            case WLOGIC_FF:  return "FF";
            case WLOGIC_B:   return "B";
            default:         return "?";
        }
    }

    // Draw circle or outline
    void drawFilledCircle(int cx, int cy, int r) {
        for (int dy = -r; dy <= r; dy++) {
            int dx = (int)sqrtf((float)(r*r - dy*dy));
            gfxLine(cx - dx, cy + dy, cx + dx, cy + dy);
        }
    }
    void DrawCircleFillOrFrame(int x, int y, bool state) {
        int r = 3;
        if (state) {
            drawFilledCircle(x, y, r);
        } else {
            // There's no built-in gfxCircle, so let's do a frame or something else.
            // For an outline circle approximation, you can do multiple lines or squares
            // but let's just do a small rectangle for outline:
            gfxFrame(x - r, y - r, r*2, r*2);
        }
    }
};
