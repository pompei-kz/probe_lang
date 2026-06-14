#pragma once
#include "Conn.h"
#include "SchemaNode.h"
#include <string>
#include <utility>
#include <vector>

std::pair<bool, std::string> test_connection(
    const std::string &host,
    const std::string &port,
    const std::string &dbname,
    const std::string &user,
    const std::string &pass
);

// Connect to DB and load repositories (schemas with lang_setting table)
std::pair<bool, std::string> connect_and_load(const Conn &c, std::vector<RepoNode> &repos);

std::pair<bool, std::string> create_repository(
    const Conn        &c,
    const std::string &schema,
    const std::string &repo_name
);

std::pair<bool, std::string> edit_repository(
    const Conn        &c,
    const std::string &old_schema,
    const std::string &new_schema,
    const std::string &new_repo_name
);
