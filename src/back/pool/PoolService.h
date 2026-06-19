//
// Created by pompei on 2026-06-19.
//

#pragma once
#include "PoolManager.h"
#include "back/model/Conn.h"

// Единая на процесс точка доступа к пулу соединений с БД.
// Сервисы берут соединения через acquire(); фронт закрывает пул отдельного
// соединения через closeConnectionPool() (при отсоединении в дереве) и все пулы
// через closeAllConnectionPools() (при завершении приложения).
namespace back::pool {

  // Единственный на процесс менеджер пулов.
  PoolManager &manager();

  // Взять соединение из пула для данного ключа (создаёт пул при необходимости).
  Connection acquire(const model::Conn &key);

  // Закрыть и удалить пул для конкретного соединения.
  void closeConnectionPool(const model::Conn &key);

  // Закрыть все пулы.
  void closeAllConnectionPools();

} // namespace back::pool
