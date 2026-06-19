#include "back/service/UnitServiceR.h"
#include "back/service/RepoServiceR.h" // make_cs, sql_err_msg
#include <algorithm>

namespace back {

  // Build an ILIKE pattern matching the filter's words in order: "%w1%w2%".
  // ILIKE wildcards (% _ \) inside each word are escaped so they match literally.
  static std::string words_ilike_pattern(const std::string &filter)
  {
    std::string pattern = "%";
    std::string word;
    auto        flush = [&] {
      if (word.empty()) return;
      for (char ch : word) {
        if (ch == '%' || ch == '_' || ch == '\\') pattern += '\\';
        pattern += ch;
      }
      pattern += '%';
      word.clear();
    };
    for (char ch : filter) {
      if (ch == ' ' || ch == '\t' || ch == '\n')
        flush();
      else
        word += ch;
    }
    flush();
    return pattern;
  }

  std::pair<std::vector<model::Unit>, std::string> list_units_paginated(
      const model::ConnStore &c, const std::string &schema, const std::string &filter, int offset, int limit)
  {
    try {
      pqxx::connection pg(make_cs(c));
      pqxx::work       txn(pg);

      pqxx::result check = txn.exec("SELECT 1 FROM information_schema.tables "
                                    "WHERE table_schema = $1 AND table_name = 'unit' LIMIT 1",
                                    pqxx::params{txn, schema});
      if (check.empty()) return {{}, ""};

      pqxx::result rows = txn.exec("SELECT id, COALESCE(parent_folder_id, ''), name, type "
                                   "FROM " +
                                       pg.quote_name(schema) +
                                       ".unit "
                                       "WHERE name ILIKE $1 "
                                       "ORDER BY name, id "
                                       "OFFSET $2 LIMIT $3",
                                   pqxx::params{txn, words_ilike_pattern(filter), std::max(0, offset), std::max(0, limit)});

      std::vector<model::Unit> units;
      for (const auto &row : rows) {
        model::Unit u{};
        u.id               = row[0].c_str();
        u.parent_folder_id = row[1].c_str();
        u.name             = row[2].c_str();
        u.type             = model::unit_type_from_string(row[3].c_str());
        units.push_back(std::move(u));
      }
      return {std::move(units), ""};
    } catch (const pqxx::sql_error &e) {
      return {{}, sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {{}, e.what()};
    }
  }

  std::vector<model::Unit> load_units_for_schema(pqxx::work &txn, const pqxx::connection &pg, const std::string &sch)
  {
    // Check the unit table exists first.
    pqxx::result check = txn.exec("SELECT 1 FROM information_schema.tables "
                                  "WHERE table_schema = $1 AND table_name = 'unit' LIMIT 1",
                                  pqxx::params{txn, sch});
    if (check.empty()) return {};

    pqxx::result rows = txn.exec("SELECT id, COALESCE(parent_folder_id, ''), name, type "
                                 "FROM " +
                                 pg.quote_name(sch) + ".unit ORDER BY name");

    std::vector<model::Unit> units;
    for (const auto &row : rows) {
      model::Unit u{};
      u.id               = row[0].c_str();
      u.parent_folder_id = row[1].c_str();
      u.name             = row[2].c_str();
      u.type             = model::unit_type_from_string(row[3].c_str());
      units.push_back(std::move(u));
    }
    return units;
  }

} // namespace back
