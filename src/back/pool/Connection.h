//
// Created by pompei on 2026-06-19.
//

#pragma once
#include <memory>
#include <pqxx/pqxx>
#include "Pool.h"

namespace back::pool {

  class Pool;

  class Connection
  {
    std::shared_ptr<Pool>             pool_{};
    std::unique_ptr<pqxx::connection> conn_{};

  public:
    Connection() = default;

    Connection(std::shared_ptr<Pool> pool, std::unique_ptr<pqxx::connection> conn)
        : pool_(std::move(pool))
        , conn_(std::move(conn))
    {}

    Connection(Connection &&)            = default;
    Connection &operator=(Connection &&) = default;

    ~Connection();

    pqxx::connection &operator*() const { return *conn_; }

    pqxx::connection *operator->() const { return conn_.get(); }
  };

} // namespace back::pool
