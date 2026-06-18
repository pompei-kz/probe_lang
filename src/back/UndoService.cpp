//
// Created by pompei on 2026-06-18.
//

#include "UndoService.h"

namespace back {
  UndoService::UndoService(pqxx::work &txn, pqxx::connection &pg, const std::string &schema)
      : txn_(txn)
      , pg_(pg)
      , schema_(schema)
  {}

  std::vector<model::UndoRowChange> UndoService::collectUndoChanges(std::vector<model::UndoRowChange> userChanges)
  {
    std::vector<model::UndoRowChange> result;



    return result;
  }
} // namespace back