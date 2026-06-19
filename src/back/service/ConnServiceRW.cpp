#include "back/service/ConnServiceRW.h"
#include "back/service/ConnServiceR.h"
#include <cstdlib>
#include <fstream>

namespace back {

  using namespace model;

  namespace fs = std::filesystem;

  static const char *EXT = ".pg-connect";

  void delete_conn(const std::string &name)
  {
    fs::remove(ws_dir() / (name + EXT));
  }

  void save_conn(const Conn &c, const std::string &old_name)
  {
    const std::filesystem::path d = ws_dir();

    fs::create_directories(d);

    if (!old_name.empty() && old_name != c.name) fs::remove(d / (old_name + EXT));

    std::ofstream f(d / (c.name + EXT));

    f << "host=" << c.host << "\nport=" << c.port << "\ndbname=" << c.dbname << "\nuser=" << c.user << "\npass=" << c.pass
      << "\nconnected=" << (c.connected ? "YES" : "NO") << "\n";
  }

} // namespace back
