//
// Created by pompei on 2026-06-19.
//

#include "Pool.h"

#include "back/service/RepoServiceR.h" // back::make_cs

namespace back::pool {

  Pool::Pool(const model::Conn &key, std::size_t maxConnections)
      : key_(key)
      , maxConnections_(maxConnections)
  {}

  Connection Pool::acquire()
  {
    std::unique_lock lock(mutex_);

    for (;;) {
      // Подождать, пока в пуле освободится соединение либо появится право открыть новое.
      cv_.wait(lock, [this] { return !free_.empty() || opened_ < maxConnections_; });

      if (!free_.empty()) {
        std::unique_ptr<pqxx::connection> conn = std::move(free_.front());
        free_.pop();
        // Соединение из пула могло порваться с прошлого использования — проверяем.
        if (conn && conn->is_open()) return Connection(shared_from_this(), std::move(conn));

        // Битое/закрытое соединение: выбрасываем, освобождая слот, и пробуем снова.
        if (opened_ > 0) --opened_;
        continue;
      }

      // Свободных соединений нет, но слот доступен — открываем новое.
      ++opened_;
      lock.unlock();
      try {

        std::unique_ptr<pqxx::connection> conn = std::make_unique<pqxx::connection>(make_cs(key_));

        return Connection(shared_from_this(), std::move(conn));
      } catch (...) {
        // Открыть не удалось: возвращаем слот и будим следующего ожидающего.
        lock.lock();
        if (opened_ > 0) --opened_;
        lock.unlock();
        cv_.notify_one();
        throw;
      }
    }
  }

  void Pool::close()
  {
    {
      std::lock_guard lock(mutex_);
      // Закрываем все простаивающие соединения (деструктор unique_ptr закрывает их).
      // Занятые соединения закроются, когда их вернут через release().
      while (!free_.empty()) {
        free_.pop();
        if (opened_ > 0) --opened_;
      }
    }
    cv_.notify_all();
  }

  void Pool::release(std::unique_ptr<pqxx::connection> conn)
  {
    {
      std::lock_guard lock(mutex_);

      if (conn && conn->is_open()) {
        free_.push(std::move(conn)); // живое — возвращаем в пул для повторного использования
      } else if (opened_ > 0) {
        --opened_; // битое/закрытое — больше не существует, освобождаем слот
      }
    }
    cv_.notify_one();
  }

} // namespace back::pool
