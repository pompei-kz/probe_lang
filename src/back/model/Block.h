#pragma once
#include <string>
#include <vector>

namespace back::model {

  // A Block is one row of unit_bl, displayed as a box on the editor canvas.
  // It is either a Method (extra fields in unit_bl_method) or a Field
  // (extra fields in unit_bl_field), discriminated by `type`.
  enum class BlockType { Method, Field };

  // One argument of a method block (a row of unit_bl_method_arg). Drawn as a
  // row below the method's badge; absent for field blocks.
  struct MethodArg
  {
    std::string id;
    std::string name;
    double      order_index = 0; // sort key within the owning method
  };

  struct Block
  {
    std::string            id;
    std::string            unit_id;          // owning unit (unit.id)
    BlockType              type = BlockType::Method;
    float                  x = 0, y = 0;     // top-left, world coordinates
    float                  width = 0, height = 0;
    std::string            name;             // from unit_bl_method / unit_bl_field
    std::vector<MethodArg> args;             // method arguments, ordered (methods only)
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
