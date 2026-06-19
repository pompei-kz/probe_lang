#pragma once
#include "TestDb.h"
#include "back/model/Conn.h"
#include "back/pool/PoolService.h"
#include <gtest/gtest.h>
#include <pqxx/pqxx>

#include <chrono>
#include <memory>
#include <string>
#include <vector>

// Base fixture for DB-backed backend tests.
//
// Every test gets its own schema named "<TestName>_<microsecond-timestamp>".
// On success the schema is dropped in TearDown so the shared test DB doesn't
// grow without bound; on failure it is kept so the resulting state can be
// inspected. The timestamp keeps each run unique, so repeated runs never collide.
//
// The schema is NOT created automatically — call make_schema() when a test
// needs an empty schema up front (some code under test creates it itself).
class DbTest : public ::testing::Test
{
protected:
  // The per-test connection is taken from the shared connection pool (PoolService),
  // the same path the services use. `pg` points at the pooled connection; the
  // handle returns it to the pool when the fixture is torn down.
  back::pool::Connection   poolConn;
  pqxx::connection        *pg = nullptr;
  std::string              schema;  // primary per-test schema name
  std::vector<std::string> schemas; // every schema this test creates; dropped in TearDown

  void SetUp() override
  {
    try {
      poolConn = back::pool::acquire(test_db::conn());
      pg       = &*poolConn;
    } catch (const std::exception &e) {
      GTEST_SKIP() << "test DB unavailable: " << e.what();
    }

    const ::testing::TestInfo *info = ::testing::UnitTest::GetInstance()->current_test_info();
    const long us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    schema        = std::string(info ? info->name() : "unknown") + "_" + std::to_string(us);
    schemas.push_back(schema); // track the primary schema for cleanup
  }

  // Record an additional schema the test creates (e.g. a second or renamed
  // schema) so TearDown drops it too. Returns the name for convenience.
  std::string track_schema(const std::string &name)
  {
    schemas.push_back(name);
    return name;
  }

  void TearDown() override
  {
    // Drop every schema this test created on success, so the shared test DB
    // doesn't grow without bound; keep them on failure for inspection.
    if (pg && !::testing::Test::HasFailure()) {
      for (const std::string &s : schemas) {
        try {
          pqxx::work txn(*pg);
          txn.exec("DROP SCHEMA IF EXISTS " + txn.quote_name(s) + " CASCADE");
          txn.commit();
        } catch (const std::exception &) {
          // best-effort cleanup — never fail a passing test on teardown
        }
      }
    }
  }

  // The test database as a back::model::Conn (for service-level APIs).
  back::model::Conn conn() const { return test_db::conn(); }

  // Create the (empty) per-test schema.
  void make_schema() const
  {
    pqxx::work txn(*pg);
    txn.exec("CREATE SCHEMA " + txn.quote_name(schema));
    txn.commit();
  }

  // Fully-qualified, quoted "schema"."table".
  std::string qual(const std::string &table) const { return pg->quote_name(schema) + "." + pg->quote_name(table); }

  bool schema_exists(const std::string &name) const
  {
    pqxx::work txn(*pg);
    return !txn.exec("SELECT 1 FROM information_schema.schemata WHERE schema_name=$1", pqxx::params{txn, name}).empty();
  }

  bool table_exists(const std::string &table)
  {
    pqxx::work txn(*pg);
    return !txn.exec("SELECT 1 FROM information_schema.tables WHERE table_schema=$1 AND table_name=$2", pqxx::params{txn, schema, table}).empty();
  }

  bool column_exists(const std::string &table, const std::string &col)
  {
    pqxx::work txn(*pg);
    return !txn.exec("SELECT 1 FROM information_schema.columns "
                     "WHERE table_schema=$1 AND table_name=$2 AND column_name=$3",
                     pqxx::params{txn, schema, table, col})
                .empty();
  }
};
