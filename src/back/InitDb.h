#pragma once
#include <pqxx/pqxx>
#include <string>

// Database schema initialization (DDL). Idempotent: every function uses
// CREATE ... IF NOT EXISTS, so it is safe to call on an already-initialized DB.
// Internal backend helper — not part of the public service API.
namespace back {

  // Ensure a repository schema and all of its tables (lang_setting, folder) exist.
  void init_repo_schema(pqxx::work &txn, pqxx::connection &pg, const std::string &schema);

  // Ensure just the folder table exists (for repos created before the folder
  // feature was introduced).
  void init_folder_table(pqxx::work &txn, pqxx::connection &pg, const std::string &schema);

  // Ensure just the unit table exists (for repos created before the unit
  // feature was introduced).
  void init_unit_table(pqxx::work &txn, pqxx::connection &pg, const std::string &schema);

  void init_unit_b_tables(pqxx::work &txn, pqxx::connection &pg, const std::string &schema);

} // namespace back
