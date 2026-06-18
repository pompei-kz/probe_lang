#pragma once
#include "BlockType.h"
#include "ExprType.h"
#include "MethodAccess.h"
#include "MethodArg.h"
#include "MethodType.h"
#include <string>
#include <vector>

namespace back::model {

  struct Block
  {
    std::string            id;
    std::string            unit_id;          // owning unit (unit.id)
    BlockType              type = BlockType::Method;
    float                  x = 0, y = 0;     // top-left, world coordinates
    float                  width = 0, height = 0;
    std::string            name;             // from unit_b_method / unit_b_field
    std::vector<MethodArg> args;             // method arguments, ordered (methods only)
    // disabled/access come from unit_b_method or unit_b_field per `type`.
    bool                   disabled = false;
    MethodType             method_type = MethodType::Inner; // method-only; ignored for fields
    MethodAccess           access = MethodAccess::Private;
    bool                   expr_id_used = false; // field-only: use the type expression instead of size_bytes
    int                    size_bytes   = 0;     // field-only: explicit size when expr_id_used is false

    // Field-only expression render data, resolved through soft references
    // (expr_id -> unit_e, and for Unit: unit_e_unit.unit_id -> unit). When
    // expr_present is false the field has no resolvable expression.
    bool     expr_present      = false;
    ExprType expr_type         = ExprType::ThisObject;
    bool     expr_unit_present = false; // (Unit) the referenced unit resolves
    std::string expr_unit_name;         // (Unit) resolved unit.name

    // Field-only expression slot: the rectangle reserved for the expression,
    // relative to this block's x/y. The block is drawn using this slot; the
    // expression itself (unit_e) is drawn separately at the matching world rect.
    // expr_has_rect is false until a slot has been stored.
    bool  expr_has_rect = false;
    float expr_x = 0, expr_y = 0, expr_width = 0, expr_height = 0;
  };

} // namespace back::model
