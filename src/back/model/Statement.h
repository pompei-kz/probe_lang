#pragma once
#include <string>

namespace back::model {

  // A Statement is one row of unit_st, displayed as a box on the editor canvas.
  // It is either a Method (extra fields in unit_st_method) or a Field
  // (extra fields in unit_st_field), discriminated by `type`.
  enum class StatementType { Method, Field };

  struct Statement
  {
    std::string   id;
    std::string   unit_id;                  // owning unit (unit.id)
    StatementType type = StatementType::Method;
    float         x = 0, y = 0;             // top-left, world coordinates
    float         width = 0, height = 0;
    std::string   name;                     // from unit_st_method / unit_st_field
  };

  inline const char *to_string(StatementType t)
  {
    switch (t) {
      case StatementType::Field: return "Field";
      case StatementType::Method:
      default: return "Method";
    }
  }

  inline StatementType statement_type_from_string(const std::string &s)
  {
    if (s == "Field") return StatementType::Field;
    return StatementType::Method;
  }

} // namespace back::model
