//
// Created by pompei on 2026-06-19.
//

#include "Connection.h"

namespace back::pool {
  Connection::~Connection()
  {
    if (pool_ && conn_) {
      pool_->release(std::move(conn_));
    }
  }
} // namespace back::pool