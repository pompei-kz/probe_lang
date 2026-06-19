#pragma once
#include "back/model/Conn.h"
#include "back/model/Unit.h"
#include <pqxx/pqxx>
#include <string>
#include <utility>
#include <vector>

namespace back {

  // Load all units of a schema as a flat list (empty if the table is absent).
  std::vector<model::Unit> load_units_for_schema(pqxx::work &txn, const pqxx::connection &pg, const std::string &sch);

  // One page of a schema's units, name-ordered, for the unit-selection form.
  // `filter` matches units whose name contains the filter's whitespace-separated
  // words in order (case-insensitive); empty filter matches everything. Returns
  // up to `limit` units starting at `offset` (a full page hints there may be more).
  std::pair<std::vector<model::Unit>, std::string> list_units_paginated(
      const model::Conn &c, const std::string &schema, const std::string &filter, int offset, int limit);

} // namespace back
