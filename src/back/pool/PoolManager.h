//
// Created by pompei on 2026-06-19.
//

#pragma once
#include "Connection.h"
#include "Pool.h"
#include "back/model/Conn.h"
#include "back/model/ConnHash.h"

#include <memory>
#include <mutex>
#include <unordered_map>

namespace back::pool {

  class PoolManager
  {
    std::mutex mutex_;

    std::size_t maxConnections_;

    std::unordered_map<model::Conn, std::shared_ptr<Pool>, model::ConnHash> pools_;

  public:
    PoolManager();

    explicit PoolManager(std::size_t maxConnections);

    Connection acquire(const model::Conn &key);

    void close(const model::Conn &key);

    void closeAll();
  };

} // namespace back::pool
