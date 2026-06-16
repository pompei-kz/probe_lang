#pragma once

namespace back::model {

  // Persisted editor coordinate-system state for one unit (zoom + offset).
  struct EditorCoordState
  {
    double zoom  = 1.0;
    double cam_x = 0.0; // world coordinate at the canvas top-left
    double cam_y = 0.0;
  };

} // namespace back::model
