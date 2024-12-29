#include <cstdint>
#include "../HSWindowsExpLinker.h"  // Access the 6 booleans from Windows

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
    const char* applet_name() override { return "WindowExp"; }
    const uint8_t* applet_icon() override { return nullptr; }

    void Start() override {
        gate_mode_2_ = WLOGIC_A;   // default for GateOut(2)
        gate_mode_3_ = WLOGIC_B;   // default for GateOut(3)
        param_index_ = 0;
        editing_     = false;
    }

    // Main loop
    void Controller() override {
        // Read the booleans from WindowsLinker
        bool a   = WindowsLinker::win_a;
        bool an  = WindowsLinker::win_and;
        bool o   = WindowsLinker::win_or;
        bool x   = WindowsLinker::win_xor;
        bool ff  = WindowsLinker::win_ff;
        bool b   = WindowsLinker::win_b;

        // Evaluate gate 2
        bool g2 = pickLogic(a, an, o, x, ff, b, gate_mode_2_);
        // Evaluate gate 3
        bool g3 = pickLogic(a, an, o, x, ff, b, gate_mode_3_);

        // Output
        GateOut(2, g2);
        GateOut(3, g3);
    }

    // Display
    void View() override {
        // Show some minimal UI
        gfxPos(0, 10);
        gfxPrint("G2: ");
        gfxPrint(logicOpName(gate_mode_2_));
        gfxPos(0, 20);
        gfxPrint("G3: ");
        gfxPrint(logicOpName(gate_mode_3_));

        // Param index text
        gfxPos(0, 30);
        gfxPrint((param_index_ == 0) ? "Edit G2" : "Edit G3");
        if (editing_) gfxPrint("*");
    }

    // Button toggles editing
    void OnButtonPress() override {
        editing_ = !editing_;
    }

    // Encoder
    void OnEncoderMove(int direction) override {
        if (!editing_) {
            // If not in editing mode, switch param index [0..1]
            param_index_ += direction;
            if (param_index_ < 0) param_index_ = 1;
            if (param_index_ > 1) param_index_ = 0;
        } else {
            // If editing
            if (param_index_ == 0) {
                // editing gate_mode_2_
                gate_mode_2_ = (WindowExpLogicOp)( (int)gate_mode_2_ + direction );
                wrapGateMode(gate_mode_2_);
            } else {
                gate_mode_3_ = (WindowExpLogicOp)( (int)gate_mode_3_ + direction );
                wrapGateMode(gate_mode_3_);
            }
        }
    }

    // Data storage
    uint64_t OnDataRequest() override {
        uint64_t data = 0;
        // gate_mode_2_ in 3 bits, gate_mode_3_ in next 3 bits (6 bits total)
        Pack(data, PackLocation{0,3}, gate_mode_2_);
        Pack(data, PackLocation{3,3}, gate_mode_3_);
        return data;
    }
    void OnDataReceive(uint64_t data) override {
        gate_mode_2_ = (WindowExpLogicOp)Unpack(data, PackLocation{0,3});
        gate_mode_3_ = (WindowExpLogicOp)Unpack(data, PackLocation{3,3});
    }

protected:
    void SetHelp() override {
        help[HELP_DIGITAL1] = "G2 Logic";
        help[HELP_DIGITAL2] = "G3 Logic";
        help[HELP_CV1]      = "";
        help[HELP_CV2]      = "";
        help[HELP_OUT1]     = "GateOut2";
        help[HELP_OUT2]     = "GateOut3";
    }

private:
    // The selected logic for gate2 / gate3
    WindowExpLogicOp gate_mode_2_;
    WindowExpLogicOp gate_mode_3_;
    // UI
    int param_index_;
    bool editing_;

    // Helper to pick the correct boolean from the 6 booleans
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

    // Wrap logic mode
    void wrapGateMode(WindowExpLogicOp &m) {
        if (m < 0)           m = (WindowExpLogicOp)(WLOGIC_COUNT - 1);
        if (m >= WLOGIC_COUNT) m = (WindowExpLogicOp)0;
    }

    // Show name
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
};
