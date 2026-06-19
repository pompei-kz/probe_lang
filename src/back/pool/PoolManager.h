//
// Created by pompei on 2026-06-19.
//

#pragma once
#include "Connection.h"
#include "back/model/Conn.h"
#include "back/model/ConnHash.h"

namespace back::pool {

  class PoolManager
  {
  public:
    Connection acquire(const model::Conn &key);

    void close(const model::Conn &key);

    void closeAll();

  private:
    std::mutex mutex_;

    std::unordered_map<model::Conn, std::shared_ptr<Pool>, model::ConnHash> pools_;
  };

} // namespace back::pool
