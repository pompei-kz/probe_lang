#pragma once
#include "Conn.h"
#include "FolderNode.h"
#include <pqxx/pqxx>
#include <string>
#include <vector>

// Internal helpers shared between the DB translation units (Db.cpp / FolderDb.cpp).
// Not part of the public backend API.
namespace back {

  // Build a libpq connection string from a Conn.
  std::string make_cs(const Conn &c);

  // Format a pqxx::sql_error including the offending query.
  std::string sql_err_msg(const pqxx::sql_error &e);

  // Load all folders for a schema into a tree (root folders returned).
  std::vector<FolderNode> load_folders_for_schema(pqxx::work &txn, pqxx::connection &pg, const std::string &sch);

} // namespace back
