#pragma once
#include <pqxx/pqxx>
#include <string>

// Database schema initialization (DDL). Idempotent: every method uses
// CREATE ... IF NOT EXISTS, so it is safe to call on an already-initialized DB.
// Internal backend helper — not part of the public service API.
namespace back {

  // Holds (but does not own) the transaction, connection and schema name used
  // by every DDL method. Construct one per schema, then call the init methods.
  class InitDb
  {
    pqxx::work        &txn_;
    pqxx::connection  &pg_;
    const std::string &schema_;

  public:
    InitDb(pqxx::work &txn, pqxx::connection &pg, const std::string &schema);

    // Ensure a repository schema and all of its tables (lang_setting, folder) exist.
    void init_repo_schema() const;

    // Ensure just the folder table exists (for repos created before the folder
    // feature was introduced).
    void init_folder_table() const;

    // Ensure just the unit table exists (for repos created before the unit
    // feature was introduced).
    void init_unit_table() const;

    void init_unit_b_tables() const;

    void init_unit_e_tables() const;

    void init_undo_tables() const;
  };

} // namespace back
