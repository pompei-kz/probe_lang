#pragma once
#include <string>

struct SchemaNode {
    std::string schema_name;
    std::string repo_name;  // value from lang_setting where name='name'
};
