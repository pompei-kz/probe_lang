#pragma once
#include <cstdlib>
#include <string>

// Single source of truth for the integration-test database connection.
// All DB-backed tests should obtain their DSN from here.
//
// The DSN can be overridden at runtime via the PROBE_LANG_TEST_DSN env var,
// which is handy for CI or pointing the suite at a different server.
namespace test_db {

  inline std::string dsn()
  {
    if (const char *env = std::getenv("PROBE_LANG_TEST_DSN")) return env;
    return "host=localhost port=18402 dbname=probe_lang_test_db_01 "
           "user=probe_lang_test_user_01 password=MHvYBQ2u2uJgmPCfePvQ "
           "connect_timeout=5";
  }

} // namespace test_db
