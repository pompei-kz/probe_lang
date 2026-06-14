#pragma once
#include <string>
#include <cstdint>

struct TxSnapshot {
    std::string buf;
    int32_t     cursor;
    int32_t     sel_start;
};
