//
// Created by pompei on 2026-06-18.
//

#pragma once
#include "model/UndoRowChange.h"

#include <pqxx/pqxx>
#include <string>

namespace back {

  class UndoService
  {
    pqxx::work        &txn_;
    pqxx::connection  &pg_;
    const std::string &schema_;

  public:
    UndoService(pqxx::work &txn, pqxx::connection &pg, const std::string &schema);

    /**
     * Собирает изменения, которые отменяют исходящие
     * @param userChanges Список исходящих изменений в БД
     * @return Список изменений, который отменяет эти изменения
     */
    std::vector<model::UndoRowChange> collectUndoChanges(std::vector<model::UndoRowChange> userChanges);
  };

} // namespace back
