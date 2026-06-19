//
// Created by pompei on 2026-06-19.
//

#include "PoolManager.h"
#include "Pool.h"
#include <vector>

namespace back::pool {

  Connection PoolManager::acquire(const model::Conn &key)
  {
    std::shared_ptr<Pool> pool;
    {
      std::lock_guard lock(mutex_);
      auto            it = pools_.find(key);
      if (it != pools_.end()) {
        pool = it->second;
      } else {
        pool = std::make_shared<Pool>(key, maxConnections_);
        pools_.emplace(key, pool);
      }
    }
    // Брать соединение нужно ВНЕ мьютекса менеджера: acquire() может заблокироваться,
    // ожидая освобождения соединения, и не должен держать общий замок пулов.
    return pool->acquire();
  }

  void PoolManager::close(const model::Conn &key)
  {
    std::shared_ptr<Pool> pool;
    {
      std::lock_guard lock(mutex_);
      auto            it = pools_.find(key);

      if (it == pools_.end()) return;

      pool = it->second;
      pools_.erase(it);
    }
    // Закрываем вне замка. Соединения, ещё занятые выданными Connection, удержат
    // Pool живым (через shared_ptr) и закроются, когда их вернут.
    pool->close();
  }

  void PoolManager::closeAll()
  {
    std::vector<std::shared_ptr<Pool>> pools;
    {
      std::lock_guard lock(mutex_);

      pools.reserve(pools_.size());
      for (auto &pool : pools_ | std::views::values) {
        pools.push_back(pool);
      }
      pools_.clear();
    }
    for (auto &pool : pools) {
      pool->close();
    }
  }

} // namespace back::pool
