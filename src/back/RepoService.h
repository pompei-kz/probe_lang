#pragma once
#include "model/Conn.h"
#include "model/SchemaNode.h"
#include <pqxx/pqxx>
#include <string>
#include <utility>
#include <vector>

namespace back {

  // Build a libpq connection string from a Conn.
  std::string make_cs(const model::Conn &c);

  // Format a pqxx::sql_error including the offending query.
  std::string sql_err_msg(const pqxx::sql_error &e);

  std::pair<bool, std::string> test_connection(
      const std::string &host, const std::string &port, const std::string &dbname, const std::string &user, const std::string &pass);

  // Connect and load repos + their folders and units
  std::pair<bool, std::string> connect_and_load(const model::Conn &c, std::vector<model::RepoNode> &repos);

  // Reload one repo's folder tree and units into `repo` (open flags reset).
  std::pair<bool, std::string> load_repo_tree(const model::Conn &c, const std::string &schema, model::RepoNode &repo);

  // Create a repository: a schema with the lang_setting and folder tables
  std::pair<bool, std::string> create_repository(const model::Conn &c, const std::string &schema, const std::string &repo_name);

  std::pair<bool, std::string> edit_repository(const model::Conn &c,
                                               const std::string &old_schema,
                                               const std::string &new_schema,
                                               const std::string &new_repo_name);

} // namespace back
