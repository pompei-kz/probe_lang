#pragma once

namespace back::model {

  // Axis-aligned bounding box in world coordinates.
  struct BBox
  {
    float min_x, min_y, max_x, max_y;
  };

} // namespace back::model
