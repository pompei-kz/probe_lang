#include "back/etc/UnitEditorState.h"

#include <cstdlib>
#include <fstream>
#include <sstream>

namespace back {

  namespace fs = std::filesystem;

  static const char *PROG = "probe_lang";

  fs::path unit_editor_sys_coord_dir()
  {
    const char *home = std::getenv("HOME");
    return fs::path(home ? home : ".") / ".config" / PROG / "unit_editor_sys_coord";
  }

  // Format a double with enough precision to round-trip, without a trailing
  // exponent for the common small values.
  static std::string fmt(double v)
  {
    std::ostringstream os;
    os.precision(12);
    os << v;
    return os.str();
  }

  void save_unit_editor_coord(const std::string &unit_id, const model::EditorCoordState &st)
  {
    const fs::path  dir = unit_editor_sys_coord_dir();
    std::error_code ec;
    fs::create_directories(dir, ec);

    std::ofstream f(dir / unit_id, std::ios::trunc);
    if (!f) return;
    f << "zoom=" << fmt(st.zoom) << '\n' << "cam_x=" << fmt(st.cam_x) << '\n' << "cam_y=" << fmt(st.cam_y) << '\n';
  }

  std::optional<model::EditorCoordState> load_unit_editor_coord(const std::string &unit_id)
  {
    std::ifstream f(unit_editor_sys_coord_dir() / unit_id);
    if (!f) return std::nullopt;

    model::EditorCoordState st;
    std::string             line;
    while (std::getline(f, line)) {
      const auto eq = line.find('=');
      if (eq == std::string::npos) continue;
      const std::string key = line.substr(0, eq);
      const std::string val = line.substr(eq + 1);
      try {
        if (key == "zoom")
          st.zoom = std::stod(val);
        else if (key == "cam_x")
          st.cam_x = std::stod(val);
        else if (key == "cam_y")
          st.cam_y = std::stod(val);
      } catch (const std::exception &) {
        // ignore malformed values, keep the default
      }
    }
    return st;
  }

} // namespace back
