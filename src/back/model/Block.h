#pragma once
#include <string>

namespace back::model {

  // A Block is one row of unit_bl, displayed as a box on the editor canvas.
  // It is either a Method (extra fields in unit_bl_method) or a Field
  // (extra fields in unit_bl_field), discriminated by `type`.
  enum class BlockType { Method, Field };

  struct Block
  {
    std::string   id;
    std::string   unit_id;                  // owning unit (unit.id)
    BlockType     type = BlockType::Method;
    float         x = 0, y = 0;             // top-left, world coordinates
    float         width = 0, height = 0;
    std::string   name;                     // from unit_bl_method / unit_bl_field
  };

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
