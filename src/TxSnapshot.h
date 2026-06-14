#pragma once
#include <cstdint>
#include <string>

struct TxSnapshot
{
  std::string buf;
  int32_t     cursor;
  int32_t     sel_start;
};
