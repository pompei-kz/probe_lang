#include "UnitService.h"
#include "CustomId.h"
#include "InitDb.h"
#include "RepoService.h" // make_cs, sql_err_msg
#include <algorithm>

namespace back {

  using namespace model;

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

  std::pair<std::vector<Unit>, std::string> list_units_paginated(
      const Conn &c, const std::string &schema, const std::string &filter, int offset, int limit)
  {
    try {
      pqxx::connection pg(make_cs(c));
      pqxx::work       txn(pg);

      pqxx::result check = txn.exec_params("SELECT 1 FROM information_schema.tables "
                                           "WHERE table_schema = $1 AND table_name = 'unit' LIMIT 1",
                                           schema);
      if (check.empty()) return {{}, ""};

      pqxx::result rows = txn.exec_params("SELECT id, COALESCE(parent_folder_id, ''), name, type "
                                          "FROM " + pg.quote_name(schema) + ".unit "
                                          "WHERE name ILIKE $1 "
                                          "ORDER BY name, id "
                                          "OFFSET $2 LIMIT $3",
                                          words_ilike_pattern(filter),
                                          std::max(0, offset),
                                          std::max(0, limit));

      std::vector<Unit> units;
      for (const auto &row : rows) {
        Unit u{};
        u.id               = row[0].c_str();
        u.parent_folder_id = row[1].c_str();
        u.name             = row[2].c_str();
        u.type             = unit_type_from_string(row[3].c_str());
        units.push_back(std::move(u));
      }
      return {std::move(units), ""};
    } catch (const pqxx::sql_error &e) {
      return {{}, sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {{}, e.what()};
    }
  }

  std::vector<Unit> load_units_for_schema(pqxx::work &txn, const pqxx::connection &pg, const std::string &sch)
  {
    // Check the unit table exists first.
    pqxx::result check = txn.exec_params("SELECT 1 FROM information_schema.tables "
                                         "WHERE table_schema = $1 AND table_name = 'unit' LIMIT 1",
                                         sch);
    if (check.empty()) return {};

    pqxx::result rows = txn.exec("SELECT id, COALESCE(parent_folder_id, ''), name, type "
                                 "FROM " +
                                 pg.quote_name(sch) + ".unit ORDER BY name");

    std::vector<Unit> units;
    for (const auto &row : rows) {
      Unit u{};
      u.id               = row[0].c_str();
      u.parent_folder_id = row[1].c_str();
      u.name             = row[2].c_str();
      u.type             = unit_type_from_string(row[3].c_str());
      units.push_back(std::move(u));
    }
    return units;
  }

  std::pair<bool, std::string> create_unit(
      const Conn &c, const std::string &schema, const std::string &parent_folder_id, const std::string &name, UnitType type)
  {
    try {
      pqxx::connection pg(make_cs(c));
      pqxx::work       txn(pg);
      std::string      schemaQuote = pg.quote_name(schema);

      // Create table in case repo predates the unit feature.
      InitDb(txn, pg, schema).init_unit_table();

      std::string       id       = new_id();
      const std::string type_str = to_string(type);
      if (parent_folder_id.empty()) {
        txn.exec_params("INSERT INTO " + schemaQuote + ".unit (id, name, type) VALUES ($1, $2, $3)", id, name, type_str);
      } else {
        txn.exec_params("INSERT INTO " + schemaQuote + ".unit (id, parent_folder_id, name, type) VALUES ($1, $2, $3, $4)",
                        id,
                        parent_folder_id,
                        name,
                        type_str);
      }
      txn.commit();
      return {true, ""};
    } catch (const pqxx::sql_error &e) {
      return {false, sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {false, e.what()};
    }
  }

  std::pair<bool, std::string> edit_unit(const Conn &c, const std::string &schema, const std::string &id, const std::string &name, UnitType type)
  {
    try {
      pqxx::connection  pg(make_cs(c));
      pqxx::work        txn(pg);
      const std::string type_str = to_string(type);
      txn.exec_params("UPDATE " + pg.quote_name(schema) + ".unit SET name = $1, type = $2 WHERE id = $3", name, type_str, id);
      txn.commit();
      return {true, ""};
    } catch (const pqxx::sql_error &e) {
      return {false, sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {false, e.what()};
    }
  }

  std::pair<bool, std::string> delete_unit(const Conn &c, const std::string &schema, const std::string &id)
  {
    try {
      pqxx::connection pg(make_cs(c));
      pqxx::work       txn(pg);
      txn.exec_params("DELETE FROM " + pg.quote_name(schema) + ".unit WHERE id = $1", id);
      txn.commit();
      return {true, ""};
    } catch (const pqxx::sql_error &e) {
      return {false, sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {false, e.what()};
    }
  }

  std::pair<bool, std::string> ensure_unit_tables(const Conn &c)
  {
    try {
      pqxx::connection pg(make_cs(c));
      pqxx::work       txn(pg);

      pqxx::result schema_rows = txn.exec("SELECT table_schema FROM information_schema.tables "
                                          "WHERE table_name = 'lang_setting' "
                                          "  AND table_schema NOT IN ('pg_catalog','information_schema') "
                                          "ORDER BY table_schema");

      for (const auto &row : schema_rows) {
        const std::string schema = row[0].c_str();
        InitDb(txn, pg, schema).init_unit_table();
      }

      txn.commit();
      return {true, ""};
    } catch (const pqxx::sql_error &e) {
      return {false, sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {false, e.what()};
    }
  }

} // namespace back
