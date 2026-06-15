#include "Conn.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <fstream>

namespace back
{

namespace fs = std::filesystem;

static const char *PROG = "probe_lang";
static const char *EXT  = ".pg-connect";

fs::path ws_dir()
{
  const char *home = SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "HOME");
  return fs::path(home ? home : ".") / ".config" / PROG / "workspace";
}

std::vector<Conn> load_all()
{
  std::vector<Conn> v;
  auto              d = ws_dir();
  if (!fs::exists(d)) return v;
  std::error_code ec;
  for (auto &e : fs::directory_iterator(d, ec)) {
    if (e.path().extension() != EXT) continue;
    Conn c;
    c.name = e.path().stem().string();
    std::ifstream f(e.path());
    std::string   ln;
    while (std::getline(f, ln)) {
      auto eq = ln.find('=');
      if (eq == std::string::npos) continue;
      auto k = ln.substr(0, eq), val = ln.substr(eq + 1);
      if (k == "host")
        c.host = val;
      else if (k == "port")
        c.port = val;
      else if (k == "user")
        c.user = val;
      else if (k == "pass")
        c.pass = val;
      else if (k == "dbname")
        c.dbname = val;
      else if (k == "connected")
        c.connected = (val == "YES");
    }
    v.push_back(std::move(c));
  }
  std::sort(v.begin(), v.end(), [](auto &a, auto &b) { return a.name < b.name; });
  return v;
}

void delete_conn(const std::string &name)
{
  fs::remove(ws_dir() / (name + EXT));
}

void save_conn(const Conn &c, const std::string &old_name)
{
  auto d = ws_dir();
  fs::create_directories(d);
  if (!old_name.empty() && old_name != c.name) fs::remove(d / (old_name + EXT));
  std::ofstream f(d / (c.name + EXT));
  f << "host=" << c.host << "\nport=" << c.port << "\ndbname=" << c.dbname
    << "\nuser=" << c.user << "\npass=" << c.pass
    << "\nconnected=" << (c.connected ? "YES" : "NO") << "\n";
}

} // namespace back
