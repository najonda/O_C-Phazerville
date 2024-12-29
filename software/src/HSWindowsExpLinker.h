#ifndef HSWINDOWS_LINKER_H_
#define HSWINDOWS_LINKER_H_

// This could also be a class with static members,
// or you could store an instance pointer. For simplicity, we'll do static.

struct WindowsLinker {
  // A, AND, OR, XOR, FF, B
  // We store them as static so WindowExp can read them.
  static bool win_a;       // from Windows
  static bool win_and;     // from Windows
  static bool win_or;      // from Windows
  static bool win_xor;     // from Windows
  static bool win_ff;      // from Windows
  static bool win_b;       // from Windows
};

// Initialize them somewhere (in the .cpp or at the bottom of this header)
#endif // HSWINDOWS_LINKER_H_
