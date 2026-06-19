#pragma once
#include "back/model/ConnStore.h"
#include <cstdlib>
#include <string>

// Single source of truth for the integration-test database connection.
// All DB-backed tests should obtain their connection details from here.
namespace test_db {

  // Connection parameters — change them in this one place.
  inline constexpr const char *HOST   = "localhost";
  inline constexpr const char *PORT   = "18402";
  inline constexpr const char *DBNAME = "probe_lang_test_db_01";
  inline constexpr const char *USER   = "probe_lang_test_user_01";
  inline constexpr const char *PASS   = "MHvYBQ2u2uJgmPCfePvQ";

  // libpq DSN (for raw pqxx connections).
  // Overridable via PROBE_LANG_TEST_DSN, handy for CI or another server.
  inline std::string dsn()
  {
    if (const char *env = std::getenv("PROBE_LANG_TEST_DSN")) return env;
    return std::string("host=") + HOST + " port=" + PORT + " dbname=" + DBNAME + " user=" + USER + " password=" + PASS + " connect_timeout=5";
  }

  // The same database as a back::model::Conn (for the service-level APIs).
  inline back::model::ConnStore conn()
  {
    back::model::ConnStore c;
    c.host   = HOST;
    c.port   = PORT;
    c.dbname = DBNAME;
    c.user   = USER;
    c.pass   = PASS;
    return c;
  }

} // namespace test_db
