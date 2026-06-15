#pragma once
#include <string>

namespace back::model {

  enum class UnitType { Class, Interface, Enum };

  struct Unit
  {
    std::string id;
    std::string parent_folder_id; // empty = directly under the repository
    std::string name;
    UnitType    type = UnitType::Class;
  };

  inline const char *to_string(UnitType t)
  {
    switch (t) {
      case UnitType::Interface: return "Interface";
      case UnitType::Enum: return "Enum";
      case UnitType::Class:
      default: return "Class";
    }
  }

  inline UnitType unit_type_from_string(const std::string &s)
  {
    if (s == "Interface") return UnitType::Interface;
    if (s == "Enum") return UnitType::Enum;
    return UnitType::Class;
  }

} // namespace back::model
