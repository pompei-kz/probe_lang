#pragma once
#include "model/Conn.h"
#include "model/ExprType.h"
#include <string>
#include <utility>

namespace back {

  // Services for a field's type expression (unit_e + per-type detail tables),
  // reached through the soft reference unit_b_field.expr_id -> unit_e.id.
  // Each operation lazily creates the unit_e row if the field has none yet
  // (expr_id NULL or dangling), then points expr_id at it.

  // Set the expression to one of the "self" kinds (ThisObject / ThisUnit /
  // ThisMethod) — these are fully stored in unit_e (no detail row).
  std::pair<bool, std::string> set_field_expr_this_type(
      const model::Conn &c, const std::string &schema, const std::string &field_id, model::ExprType type);

  // Set the expression to ExprType::Unit pointing at `unit_id`: ensures the
  // unit_e row (type = 'Unit') and the unit_e_unit detail row (unit_id).
  std::pair<bool, std::string> set_field_expr_unit(
      const model::Conn &c, const std::string &schema, const std::string &field_id, const std::string &unit_id);

} // namespace back
