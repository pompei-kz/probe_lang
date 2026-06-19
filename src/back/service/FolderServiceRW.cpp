#include "back/service/FolderServiceRW.h"
#include "back/etc/CustomId.h"
#include "back/etc/InitDb.h"
#include "back/service/RepoServiceR.h"

namespace back {

  using namespace model;

  std::pair<bool, std::string> create_folder(const Conn &c, const std::string &schema, const std::string &parent_id, const std::string &name)
  {
    try {
      pqxx::connection pg(make_cs(c));
      pqxx::work       txn(pg);
      std::string      qsch = pg.quote_name(schema);

      // Create table in case repo was added before folder feature was introduced
      InitDb(txn, pg, schema).init_folder_table();

      std::string id = new_id();
      if (parent_id.empty()) {
        txn.exec("INSERT INTO " + qsch + ".folder (id, name) VALUES ($1, $2)", pqxx::params{txn, id, name});
      } else {
        txn.exec("INSERT INTO " + qsch + ".folder (id, parent_id, name) VALUES ($1, $2, $3)", pqxx::params{txn, id, parent_id, name});
      }
      txn.commit();
      return {true, ""};
    } catch (const pqxx::sql_error &e) {
      return {false, sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {false, e.what()};
    }
  }

  std::pair<bool, std::string> rename_folder(const Conn &c, const std::string &schema, const std::string &id, const std::string &new_name)
  {
    try {
      pqxx::connection pg(make_cs(c));
      pqxx::work       txn(pg);
      txn.exec("UPDATE " + pg.quote_name(schema) + ".folder SET name = $1 WHERE id = $2", pqxx::params{txn, new_name, id});
      txn.commit();
      return {true, ""};
    } catch (const pqxx::sql_error &e) {
      return {false, sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {false, e.what()};
    }
  }

  std::pair<bool, std::string> delete_folder_recursive(const Conn &c, const std::string &schema, const std::string &id)
  {
    try {
      pqxx::connection pg(make_cs(c));
      pqxx::work       txn(pg);
      std::string      qsch = pg.quote_name(schema);

      txn.exec("WITH RECURSIVE descendants AS ("
               "  SELECT id FROM " +
                   qsch +
                   ".folder WHERE id = $1 "
                   "  UNION ALL "
                   "  SELECT f.id FROM " +
                   qsch +
                   ".folder f "
                   "  JOIN descendants d ON f.parent_id = d.id"
                   ") "
                   "DELETE FROM " +
                   qsch + ".folder WHERE id IN (SELECT id FROM descendants)",
               pqxx::params{txn, id});
      txn.commit();
      return {true, ""};
    } catch (const pqxx::sql_error &e) {
      return {false, sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {false, e.what()};
    }
  }

} // namespace back
