// Copyright (c) 2016 Patrick Dowling
//
// Author: Patrick Dowling (pld@gurkenkiste.com)
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

#ifndef UTIL_SETTINGS_H_
#define UTIL_SETTINGS_H_

#include <algorithm>
#include <stdint.h>
#include <string.h>
#include "util_stream_buffer.h"
#include "util_misc.h"

namespace settings {

enum StorageType {
  STORAGE_TYPE_U4, // nibbles are packed where possible, else aligned to next byte
  STORAGE_TYPE_I8, STORAGE_TYPE_U8,
  STORAGE_TYPE_I16, STORAGE_TYPE_U16,
  STORAGE_TYPE_I32, STORAGE_TYPE_U32,
  STORAGE_TYPE_NOP,
};

struct ValueAttributes {
  int default_;
  int min_;
  int max_;
  const char *name;
  const char * const *value_names;
  StorageType storage_type;

  int default_value() const {
    return default_;
  }

  int clamp(int value) const {
    if (value < min_) return min_;
    else if (value > max_) return max_;
    else return value;
  }
};

// Implement the actual reading and writing of settings as external functions.
// This reduces some template bloat by avoiding a separate Save/Restore
// implementation for each class derived from SettingsBase.
class SettingsRW {
public:
  static size_t Write(const int values[], const ValueAttributes attributes[], size_t num_valyes, util::StreamBufferWriter &stream_writer);
  static size_t Read(int values[], const ValueAttributes attributes[], size_t num_values, util::StreamBufferReader &stream_reader);
};

// Provide a very simple "settings" base.
// Settings values are an array of ints that are accessed by index, usually the
// owning class will use an enum for clarity, and provide specific getter
// functions for each value.
//
// The values are decsribed using the per-class value_attr array and allow min,
// max and default values. To set the defaults, call ::InitDefaults at least
// once before using the class. The min/max values are enforced when setting
// or modifying values. Classes shouldn't normally have to access the values_
// directly.
//
// To try and save some storage space, each setting can be stored as a smaller
// type as specified in the attributes. For even more compact representations,
// the owning class can pack things differently if required.
//
// TODO: Save/Restore is still kind of sucky
// TODO: If absolutely necessary, add STORAGE_TYPE_BIT and pack nibbles & bits
//
template <typename clazz, size_t num_settings>
class SettingsBase {
public:

  int get_value(size_t index) const {
    return values_[index];
  }

  static int clamp_value(size_t index, int value) {
    return value_attr_[index].clamp(value);
  }

  bool apply_value(size_t index, int value) {
    if (index < num_settings) {
      const int clamped = value_attr_[index].clamp(value);
      if (values_[index] != clamped) {
        values_[index] = clamped;
        return true;
      }
    }
    return false;
  }

  bool change_value(size_t index, int delta) {
    return apply_value(index, values_[index] + delta);
  }

  static const ValueAttributes &value_attributes(size_t i) {
    return value_attr_[i];
  }

  void InitDefaults() {
    std::transform(
        std::begin(value_attr_), std::end(value_attr_),
        std::begin(values_),
        [](const ValueAttributes &attributes) { return attributes.default_value(); });
  }

  size_t Save(util::StreamBufferWriter &stream_writer) const {
    return SettingsRW::Write(values_, value_attr_, num_settings, stream_writer);
  }

  size_t Restore(util::StreamBufferReader &stream_reader) {
    return SettingsRW::Read(values_, value_attr_, num_settings, stream_reader);
  }

  static size_t storageSize() {
    return storage_size_;
  }

protected:

  int values_[num_settings];
  static const ValueAttributes value_attr_[num_settings];
  static const size_t storage_size_;

  static size_t calc_storage_size() {
    size_t s = 0;
    unsigned nibbles = 0;
    for (auto attr : value_attr_) {
      if (STORAGE_TYPE_U4 == attr.storage_type) {
        ++nibbles;
      } else {
        if (nibbles & 1) ++nibbles;
        switch(attr.storage_type) {
          case STORAGE_TYPE_I8: s += sizeof(int8_t); break;
          case STORAGE_TYPE_U8: s += sizeof(uint8_t); break;
          case STORAGE_TYPE_I16: s += sizeof(int16_t); break;
          case STORAGE_TYPE_U16: s += sizeof(uint16_t); break;
          case STORAGE_TYPE_I32: s += sizeof(int32_t); break;
          case STORAGE_TYPE_U32: s += sizeof(uint32_t); break;
          default: break;
        }
      }
    }
    if (nibbles & 1) ++nibbles;
    s += nibbles >> 1;
    return s;
  }
};

#define SETTINGS_DECLARE(clazz, last) \
template <> const size_t settings::SettingsBase<clazz, last>::storage_size_ = settings::SettingsBase<clazz, last>::calc_storage_size(); \
template <> const settings::ValueAttributes settings::SettingsBase<clazz, last>::value_attr_[] =

} // namespace settings

#endif // UTIL_SETTINGS_H_
