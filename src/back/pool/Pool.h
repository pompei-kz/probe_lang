//
// Created by pompei on 2026-06-19.
//

#pragma once
#include "Connection.h"
#include "back/model/Conn.h"

#include <condition_variable>
#include <pqxx/pqxx>

#include <memory>
#include <mutex>
#include <queue>

namespace back::pool {

  class Connection;

  class Pool : public std::enable_shared_from_this<Pool>
  {
  public:
    explicit Pool(const model::Conn &key, std::size_t maxConnections = 10);

    Connection acquire();

    void close();

  private:
    friend class Connection;

    void release(std::unique_ptr<pqxx::connection>);

    model::Conn key_;

    std::mutex              mutex_;
    std::condition_variable cv_;

    std::queue<std::unique_ptr<pqxx::connection>> free_;

    std::size_t opened_ = 0;
    std::size_t maxConnections_;
  };

} // namespace back::pool
