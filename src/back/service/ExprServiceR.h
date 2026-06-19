#pragma once
#include "back/model/ConnStore.h"
#include "back/model/Expr.h"
#include "back/model/ExprType.h"
#include <string>
#include <utility>
#include <vector>

namespace back {

  // Expressions of `unit_id` whose world rectangle intersects the viewport
  // (spatial GIST query on unit_e.geom). Drawn independently of their blocks.
  // Empty if the expression tables are absent.
  std::pair<std::vector<model::Expr>, std::string> load_exprs_in_view(
      const model::ConnStore &c, const std::string &schema, const std::string &unit_id, float min_x, float min_y, float max_x, float max_y);

} // namespace back
