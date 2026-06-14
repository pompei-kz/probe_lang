#pragma once
#include "Conn.h"
#include "FolderNode.h"
#include "SchemaNode.h"
#include <string>
#include <utility>
#include <vector>

std::pair<bool, std::string> test_connection(
    const std::string &host, const std::string &port,
    const std::string &dbname, const std::string &user, const std::string &pass);

// Connect and load repos + their root folders
std::pair<bool, std::string> connect_and_load(const Conn &c, std::vector<RepoNode> &repos);

std::pair<bool, std::string> create_repository(
    const Conn &c, const std::string &schema, const std::string &repo_name);

std::pair<bool, std::string> edit_repository(
    const Conn &c, const std::string &old_schema,
    const std::string &new_schema, const std::string &new_repo_name);

// Load root folders for one repo schema (rebuilds tree structure)
std::pair<bool, std::string> load_repo_folders(
    const Conn &c, const std::string &schema, std::vector<FolderNode> &root_folders);

// Create a new folder; parent_id empty = root
std::pair<bool, std::string> create_folder(
    const Conn &c, const std::string &schema,
    const std::string &parent_id, const std::string &name);

// Rename an existing folder
std::pair<bool, std::string> rename_folder(
    const Conn &c, const std::string &schema,
    const std::string &id, const std::string &new_name);

// Delete folder and all descendants recursively (no FK — uses recursive CTE)
std::pair<bool, std::string> delete_folder_recursive(
    const Conn &c, const std::string &schema, const std::string &id);
