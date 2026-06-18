#pragma once
#include <string>

namespace back::model {

  // Method-only attribute stored in unit_b_method.
  enum class MethodType { Inner, Static, Constructor, Destructor };

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

} // namespace back::model
