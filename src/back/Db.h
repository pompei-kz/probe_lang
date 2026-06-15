#pragma once
#include "Conn.h"
#include "SchemaNode.h"
#include <string>
#include <utility>
#include <vector>

namespace back {

  std::pair<bool, std::string> test_connection(
      const std::string &host, const std::string &port, const std::string &dbname, const std::string &user, const std::string &pass);

  // Connect and load repos + their root folders
  std::pair<bool, std::string> connect_and_load(const Conn &c, std::vector<RepoNode> &repos);

  // Create a repository: a schema with the lang_setting and folder tables
  std::pair<bool, std::string> create_repository(const Conn &c, const std::string &schema, const std::string &repo_name);

  std::pair<bool, std::string> edit_repository(const Conn        &c,
                                               const std::string &old_schema,
                                               const std::string &new_schema,
                                               const std::string &new_repo_name);

} // namespace back
