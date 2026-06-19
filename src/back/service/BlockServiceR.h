#pragma once
#include "back/model/BBox.h"
#include "back/model/Block.h"
#include "back/model/ConnStore.h"
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace back {

  // Load the blocks of one unit whose geometry intersects the given world
  // rectangle. Uses the GIST index on unit_b.geom (the `&&` bbox operator).
  // Returns an empty list (no error) when the unit_b table is absent.
  std::pair<std::vector<model::Block>, std::string> load_blocks_in_view(
      const model::ConnStore &c, const std::string &schema, const std::string &unit_id, float min_x, float min_y, float max_x, float max_y);

  // Bounding box covering all blocks of one unit, or nullopt if it has none.
  // Used to center the editor camera when it first opens.
  std::pair<std::optional<model::BBox>, std::string> block_bbox_for_unit(const model::ConnStore &c, const std::string &schema, const std::string &unit_id);

} // namespace back
