#pragma once
#include <filesystem>
#include <optional>
#include <string>

// Per-unit persistence of the editor's coordinate system (zoom + offset).
// One file per unit under ~/.config/probe_lang/unit_editor_sys_coord/, named by
// the unit id, holding "key=value" lines. Saved right after the user changes the
// zoom or pans; read when the unit is opened in the editor.
namespace back {

  struct EditorCoordState
  {
    double zoom  = 1.0;
    double cam_x = 0.0; // world coordinate at the canvas top-left
    double cam_y = 0.0;
  };

  // Directory holding the per-unit state files.
  std::filesystem::path unit_editor_sys_coord_dir();

  // Persist a unit's coordinate-system state (creates the directory as needed).
  void save_unit_editor_coord(const std::string &unit_id, const EditorCoordState &st);

  // Load a unit's saved state, or nullopt when no file exists.
  std::optional<EditorCoordState> load_unit_editor_coord(const std::string &unit_id);

} // namespace back
