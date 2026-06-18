#pragma once
#include <string>

namespace back::model {

  // A Block is one row of unit_b, displayed as a box on the editor canvas.
  // It is either a Method (extra fields in unit_b_method) or a Field
  // (extra fields in unit_b_field), discriminated by `type`.
  enum class BlockType { Method, Field };

  inline const char *to_string(BlockType t)
  {
    switch (t) {
      case BlockType::Field: return "Field";
      case BlockType::Method:
      default: return "Method";
    }
  }

  inline BlockType block_type_from_string(const std::string &s)
  {
    if (s == "Field") return BlockType::Field;
    return BlockType::Method;
  }

} // namespace back::model
