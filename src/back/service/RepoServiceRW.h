#pragma once
#include "back/model/Conn.h"
#include <string>
#include <utility>

namespace back {

  // Create a repository: a schema with the lang_setting and folder tables
  std::pair<bool, std::string> create_repository(const model::Conn &c, const std::string &schema, const std::string &repo_name);

  std::pair<bool, std::string> edit_repository(const model::Conn &c,
                                               const std::string &old_schema,
                                               const std::string &new_schema,
                                               const std::string &new_repo_name);

  // Ensure a repository schema and all of its tables exist (idempotent). Called
  // when a repository branch is opened, so repos predating newer tables get them.
  std::pair<bool, std::string> ensure_repo_schema(const model::Conn &c, const std::string &schema);

} // namespace back
