#pragma once
#include "Conn.h"
#include "SchemaNode.h"
#include <string>
#include <vector>

struct ConnNode {
    Conn                    conn;
    bool                    open = false;
    std::vector<SchemaNode> schemas;
};
