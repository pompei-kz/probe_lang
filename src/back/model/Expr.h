#pragma once
#include "ExprType.h"
#include <string>

namespace back::model {

  // A type expression (one unit_e row) drawn on the canvas independently of its
  // owning field block. x/y/width/height are world coordinates (the top-left and
  // size of the expression), kept in sync with the field's relative expr_* slot.
  struct Expr
  {
    std::string id;
    std::string owner_block_id; // the field block this expression belongs to (unit_b.id)
    ExprType    type = ExprType::ThisObject;
    float       x = 0, y = 0;
    float       width = 0, height = 0;

    // (Unit) the referenced unit, resolved through the soft reference
    // unit_e_unit.unit_id -> unit.id.
    bool        unit_present = false;
    std::string unit_name;
  };

} // namespace back::model
