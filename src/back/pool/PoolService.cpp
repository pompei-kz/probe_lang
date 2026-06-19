//
// Created by pompei on 2026-06-19.
//

#include "PoolService.h"

namespace back::pool {

  PoolManager &manager()
  {
    // Локальная static: потокобезопасная ленивая инициализация (C++11+),
    // живёт до конца программы.
    static PoolManager instance;
    return instance;
  }

  Connection acquire(const model::Conn &key)
  {
    return manager().acquire(key);
  }

  void closeConnectionPool(const model::Conn &key)
  {
    manager().close(key);
  }

  void closeAllConnectionPools()
  {
    manager().closeAll();
  }

} // namespace back::pool
