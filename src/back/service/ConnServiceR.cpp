#include "back/service/ConnServiceR.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>

namespace back {

  namespace fs = std::filesystem;

  static const char *PROG = "probe_lang";
  static const char *EXT  = ".pg-connect";

  fs::path ws_dir()
  {
    const char *home = std::getenv("HOME");
    return fs::path(home ? home : ".") / ".config" / PROG / "workspace";
  }

  std::vector<model::ConnStore> load_all()
  {
    std::vector<model::ConnStore> v;
    std::filesystem::path d = ws_dir();

    if (!fs::exists(d)) {
      return v;
    }

    // ReSharper disable once CppTooWideScopeInitStatement
    std::error_code ec;

    for (const std::filesystem::directory_entry &e : fs::directory_iterator(d, ec)) {
      if (e.path().extension() != EXT) continue;
      model::ConnStore c;
      c.name = e.path().stem().string();
      std::ifstream f(e.path());
      std::string   ln;
      while (std::getline(f, ln)) {
        std::size_t eq = ln.find('=');

        if (eq == std::string::npos) continue;

        // ReSharper disable once CppTooWideScopeInitStatement
        std::string k = ln.substr(0, eq), val = ln.substr(eq + 1);

        if (k == "host") {
          c.host = val;
        } else if (k == "port") {
          c.port = val;
        } else if (k == "user") {
          c.user = val;
        } else if (k == "pass") {
          c.pass = val;
        } else if (k == "dbname") {
          c.dbname = val;
        } else if (k == "connected") {
          c.connected = (val == "YES");
        }
      }
      v.push_back(std::move(c));
    }

    std::ranges::sort(v, [](auto &a, auto &b) { return a.name < b.name; });

    return v;
  }

} // namespace back
