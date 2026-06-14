#pragma once
#include <string>
#include <vector>

struct FolderNode {
    std::string             id;
    std::string             parent_id;  // empty = top-level
    std::string             name;
    std::vector<FolderNode> children;
    bool                    open = false;
};
