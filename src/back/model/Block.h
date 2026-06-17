#pragma once
#include <string>
#include <vector>

namespace back::model {

  // A Block is one row of unit_bl, displayed as a box on the editor canvas.
  // It is either a Method (extra fields in unit_bl_method) or a Field
  // (extra fields in unit_bl_field), discriminated by `type`.
  enum class BlockType { Method, Field };

  // Method-only attributes stored in unit_bl_method.
  enum class MethodType { Inner, Static, Constructor, Destructor };
  enum class MethodAccess { Private, Protected, Public };

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
    // disabled/access come from unit_bl_method or unit_bl_field per `type`.
    bool                   disabled = false;
    MethodType             method_type = MethodType::Inner; // method-only; ignored for fields
    MethodAccess           access = MethodAccess::Private;
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

  inline const char *to_string(MethodType t)
  {
    switch (t) {
      case MethodType::Static: return "Static";
      case MethodType::Constructor: return "Constructor";
      case MethodType::Destructor: return "Destructor";
      case MethodType::Inner:
      default: return "Inner";
    }
  }

  inline MethodType method_type_from_string(const std::string &s)
  {
    if (s == "Static") return MethodType::Static;
    if (s == "Constructor") return MethodType::Constructor;
    if (s == "Destructor") return MethodType::Destructor;
    return MethodType::Inner;
  }

  inline const char *to_string(MethodAccess a)
  {
    switch (a) {
      case MethodAccess::Protected: return "Protected";
      case MethodAccess::Public: return "Public";
      case MethodAccess::Private:
      default: return "Private";
    }
  }

  inline MethodAccess method_access_from_string(const std::string &s)
  {
    if (s == "Protected") return MethodAccess::Protected;
    if (s == "Public") return MethodAccess::Public;
    return MethodAccess::Private;
  }

} // namespace back::model
