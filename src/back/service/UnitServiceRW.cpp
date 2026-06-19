#include "back/service/UnitServiceRW.h"
#include "back/etc/CustomId.h"
#include "back/etc/InitDb.h"
#include "back/service/RepoServiceR.h" // make_cs, sql_err_msg

namespace back {

  std::pair<bool, std::string> create_unit(
      const model::Conn &c, const std::string &schema, const std::string &parent_folder_id, const std::string &name, model::UnitType type)
  {
    try {
      pqxx::connection pg(make_cs(c));
      pqxx::work       txn(pg);
      std::string      schemaQuote = pg.quote_name(schema);

      // Create table in case repo predates the unit feature.
      InitDb(txn, pg, schema).init_unit_table();

      std::string       id       = new_id();
      const std::string type_str = model::to_string(type);
      if (parent_folder_id.empty()) {
        txn.exec("INSERT INTO " + schemaQuote + ".unit (id, name, type) VALUES ($1, $2, $3)", pqxx::params{txn, id, name, type_str});
      } else {
        txn.exec("INSERT INTO " + schemaQuote + ".unit (id, parent_folder_id, name, type) VALUES ($1, $2, $3, $4)",
                 pqxx::params{txn, id, parent_folder_id, name, type_str});
      }
      txn.commit();
      return {true, ""};
    } catch (const pqxx::sql_error &e) {
      return {false, sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {false, e.what()};
    }
  }

  std::pair<bool, std::string> edit_unit(
      const model::Conn &c, const std::string &schema, const std::string &id, const std::string &name, model::UnitType type)
  {
    try {
      pqxx::connection  pg(make_cs(c));
      pqxx::work        txn(pg);
      const std::string type_str = model::to_string(type);
      txn.exec("UPDATE " + pg.quote_name(schema) + ".unit SET name = $1, type = $2 WHERE id = $3", pqxx::params{txn, name, type_str, id});
      txn.commit();
      return {true, ""};
    } catch (const pqxx::sql_error &e) {
      return {false, sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {false, e.what()};
    }
  }

  std::pair<bool, std::string> delete_unit(const model::Conn &c, const std::string &schema, const std::string &id)
  {
    try {
      pqxx::connection pg(make_cs(c));
      pqxx::work       txn(pg);
      txn.exec("DELETE FROM " + pg.quote_name(schema) + ".unit WHERE id = $1", pqxx::params{txn, id});
      txn.commit();
      return {true, ""};
    } catch (const pqxx::sql_error &e) {
      return {false, sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {false, e.what()};
    }
  }

  std::pair<bool, std::string> ensure_unit_tables(const model::Conn &c)
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
