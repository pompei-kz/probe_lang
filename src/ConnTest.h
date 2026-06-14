#pragma once
#include "Conn.h"
#include "SchemaNode.h"
#include <string>
#include <utility>
#include <vector>

// Returns {true, ""} on success (schemas filled), or {false, error_message}
std::pair<bool, std::string> test_connection(
    const std::string &host,
    const std::string &port,
    const std::string &dbname,
    const std::string &user,
    const std::string &pass
);

// Connect to DB and load schemas that have lang_setting table with name='name' record
// Returns {true, ""} on success with schemas filled, or {false, error}
std::pair<bool, std::string> connect_and_load(const Conn &c, std::vector<SchemaNode> &schemas);

// Create schema with lang_setting table, insert name=repo_name record
// Returns {true, ""} on success, or {false, error}
std::pair<bool, std::string> create_repository(
    const Conn        &c,
    const std::string &schema,
    const std::string &repo_name
);
