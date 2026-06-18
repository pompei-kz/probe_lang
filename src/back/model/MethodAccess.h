#pragma once
#include <string>

namespace back::model {

  // Method-only attribute stored in unit_b_method.
  enum class MethodAccess { Private, Protected, Public };

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
