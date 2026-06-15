#pragma once
#include "model/Conn.h"
#include "model/FolderNode.h"
#include <pqxx/pqxx>
#include <string>
#include <vector>

// Internal helpers shared between the DB services (RepoService.cpp / FolderService.cpp).
// Not part of the public backend API.
namespace back {

  // Build a libpq connection string from a Conn.
  std::string make_cs(const model::Conn &c);

  // Format a pqxx::sql_error including the offending query.
  std::string sql_err_msg(const pqxx::sql_error &e);

  // Load all folders for a schema into a tree (root folders returned).
  std::vector<model::FolderNode> load_folders_for_schema(pqxx::work &txn, pqxx::connection &pg, const std::string &sch);

} // namespace back
