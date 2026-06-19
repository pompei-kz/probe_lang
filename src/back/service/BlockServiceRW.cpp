#include "back/service/BlockServiceRW.h"
#include "back/etc/CustomId.h"
#include "back/etc/InitDb.h"
#include "back/etc/UtilDb.h" // hasTable
#include "back/pool/PoolService.h"
#include "back/service/RepoServiceR.h" // make_cs, sql_err_msg

namespace back {

  std::pair<std::string, std::string> create_block(const model::Conn &c,
                                                   const std::string &schema,
                                                   const std::string &unit_id,
                                                   model::BlockType   type,
                                                   float              x,
                                                   float              y,
                                                   float              width,
                                                   float              height,
                                                   const std::string &name)
  {
    try {
      pool::Connection  pgPool = pool::acquire(c);
      pqxx::connection &pg     = *pgPool;
      pqxx::work        txn(pg);

      // Create the unit_b tables in case the repo predates this feature.
      InitDb(txn, pg, schema).init_unit_b_tables();

      const std::string qs       = pg.quote_name(schema);
      const std::string id       = new_id();
      const std::string type_str = model::to_string(type);

      txn.exec("INSERT INTO " + qs +
                   ".unit_b (id, unit_id, type, x, y, width, height) "
                   "VALUES ($1, $2, $3, $4, $5, $6, $7)",
               pqxx::params{txn, id, unit_id, type_str, x, y, width, height});

      const std::string detail = type == model::BlockType::Field ? ".unit_b_field" : ".unit_b_method";
      txn.exec("INSERT INTO " + qs + detail + " (id, name) VALUES ($1, $2)", pqxx::params{txn, id, name});

      txn.commit();
      return {id, ""};
    } catch (const pqxx::sql_error &e) {
      return {"", sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {"", e.what()};
    }
  }

  std::pair<std::string, std::string> create_method_arg(
      const model::Conn &c, const std::string &schema, const std::string &owner_method_id, double order_index, const std::string &name)
  {
    try {
      pool::Connection  pgPool = pool::acquire(c);
      pqxx::connection &pg     = *pgPool;
      pqxx::work        txn(pg);

      // Create the unit_b tables in case the repo predates this feature.
      InitDb(txn, pg, schema).init_unit_b_tables();

      const std::string id = new_id();
      txn.exec("INSERT INTO " + pg.quote_name(schema) + ".unit_b_method_arg (id, owner_method_id, order_index, name) VALUES ($1, $2, $3, $4)",
               pqxx::params{txn, id, owner_method_id, order_index, name});
      txn.commit();
      return {id, ""};
    } catch (const pqxx::sql_error &e) {
      return {"", sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {"", e.what()};
    }
  }

  std::pair<std::string, std::string> append_method_arg(const model::Conn &c,
                                                        const std::string &schema,
                                                        const std::string &owner_method_id,
                                                        const std::string &name)
  {
    try {
      pool::Connection  pgPool = pool::acquire(c);
      pqxx::connection &pg     = *pgPool;
      pqxx::work        txn(pg);

      // Create the unit_b tables in case the repo predates this feature.
      InitDb(txn, pg, schema).init_unit_b_tables();

      // Always append at the end: order_index = max(order_index in this method) + 1.
      // Computed in the same transaction so a prior delete can't shift it.
      const std::string id = new_id();
      txn.exec("INSERT INTO " + pg.quote_name(schema) +
                   ".unit_b_method_arg (id, owner_method_id, order_index, name) "
                   "SELECT $1, $2::text, COALESCE(MAX(order_index), -1) + 1, $3 "
                   "FROM " +
                   pg.quote_name(schema) + ".unit_b_method_arg WHERE owner_method_id = $2::text",
               pqxx::params{txn, id, owner_method_id, name});
      txn.commit();
      return {id, ""};
    } catch (const pqxx::sql_error &e) {
      return {"", sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {"", e.what()};
    }
  }

  std::pair<bool, std::string> update_method_arg_name(const model::Conn &c, const std::string &schema, const std::string &id, const std::string &name)
  {
    try {
      pool::Connection  pgPool = pool::acquire(c);
      pqxx::connection &pg     = *pgPool;
      pqxx::work        txn(pg);
      txn.exec("UPDATE " + pg.quote_name(schema) + ".unit_b_method_arg SET name = $1 WHERE id = $2", pqxx::params{txn, name, id});
      txn.commit();
      return {true, ""};
    } catch (const pqxx::sql_error &e) {
      return {false, sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {false, e.what()};
    }
  }

  std::pair<bool, std::string> delete_method_arg(const model::Conn &c, const std::string &schema, const std::string &id)
  {
    try {
      pool::Connection  pgPool = pool::acquire(c);
      pqxx::connection &pg     = *pgPool;
      pqxx::work        txn(pg);
      txn.exec("DELETE FROM " + pg.quote_name(schema) + ".unit_b_method_arg WHERE id = $1", pqxx::params{txn, id});
      txn.commit();
      return {true, ""};
    } catch (const pqxx::sql_error &e) {
      return {false, sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {false, e.what()};
    }
  }

  std::pair<bool, std::string> reorder_method_args(const model::Conn              &c,
                                                   const std::string              &schema,
                                                   const std::string              &owner_method_id,
                                                   const std::vector<std::string> &ordered_ids)
  {
    try {
      pool::Connection  pgPool = pool::acquire(c);
      pqxx::connection &pg     = *pgPool;
      pqxx::work        txn(pg);
      // Rewrite order_index to match the given sequence (0..n-1), all in one
      // transaction. The owner_method_id guard keeps it scoped to this method.
      const std::string q = "UPDATE " + pg.quote_name(schema) + ".unit_b_method_arg SET order_index = $1 WHERE id = $2 AND owner_method_id = $3";
      for (int i = 0; i < static_cast<int>(ordered_ids.size()); i++)
        txn.exec(q, pqxx::params{txn, i, ordered_ids[i], owner_method_id});
      txn.commit();
      return {true, ""};
    } catch (const pqxx::sql_error &e) {
      return {false, sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {false, e.what()};
    }
  }

  // Helper: UPDATE one unit_b_method column for a single method id.
  static std::pair<bool, std::string> update_method_column(
      const model::Conn &c, const std::string &schema, const std::string &id, const std::string &column, const std::string &value)
  {
    try {
      pool::Connection  pgPool = pool::acquire(c);
      pqxx::connection &pg     = *pgPool;
      pqxx::work        txn(pg);
      // `column` is a fixed internal literal, never user input.
      txn.exec("UPDATE " + pg.quote_name(schema) + ".unit_b_method SET " + column + " = $1 WHERE id = $2", pqxx::params{txn, value, id});
      txn.commit();
      return {true, ""};
    } catch (const pqxx::sql_error &e) {
      return {false, sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {false, e.what()};
    }
  }

  std::pair<bool, std::string> update_block_disabled(const model::Conn &c, const std::string &schema, const std::string &id, bool disabled)
  {
    try {
      pool::Connection  pgPool = pool::acquire(c);
      pqxx::connection &pg     = *pgPool;
      pqxx::work        txn(pg);
      const std::string value = disabled ? "true" : "false";
      txn.exec("UPDATE " + pg.quote_name(schema) + ".unit_b SET disabled = $1 WHERE id = $2", pqxx::params{txn, value, id});
      txn.commit();
      return {true, ""};
    } catch (const pqxx::sql_error &e) {
      return {false, sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {false, e.what()};
    }
  }

  std::pair<bool, std::string> update_method_type(const model::Conn &c, const std::string &schema, const std::string &id, model::MethodType type)
  {
    return update_method_column(c, schema, id, "type", model::to_string(type));
  }

  std::pair<bool, std::string> update_method_access(const model::Conn  &c,
                                                    const std::string  &schema,
                                                    const std::string  &id,
                                                    model::MethodAccess access)
  {
    return update_method_column(c, schema, id, "access", model::to_string(access));
  }

  // Helper: UPDATE one unit_b_field column for a single field id.
  static std::pair<bool, std::string> update_field_column(
      const model::Conn &c, const std::string &schema, const std::string &id, const std::string &column, const std::string &value)
  {
    try {
      pool::Connection  pgPool = pool::acquire(c);
      pqxx::connection &pg     = *pgPool;
      pqxx::work        txn(pg);
      // `column` is a fixed internal literal, never user input.
      txn.exec("UPDATE " + pg.quote_name(schema) + ".unit_b_field SET " + column + " = $1 WHERE id = $2", pqxx::params{txn, value, id});
      txn.commit();
      return {true, ""};
    } catch (const pqxx::sql_error &e) {
      return {false, sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {false, e.what()};
    }
  }

  std::pair<bool, std::string> update_field_access(const model::Conn &c, const std::string &schema, const std::string &id, model::MethodAccess access)
  {
    return update_field_column(c, schema, id, "access", model::to_string(access));
  }

  std::pair<bool, std::string> update_field_expr_id_used(const model::Conn &c, const std::string &schema, const std::string &id, bool expr_id_used)
  {
    return update_field_column(c, schema, id, "expr_id_used", expr_id_used ? "true" : "false");
  }

  std::pair<bool, std::string> update_field_size_bytes(const model::Conn &c, const std::string &schema, const std::string &id, int size_bytes)
  {
    return update_field_column(c, schema, id, "size_bytes", std::to_string(size_bytes));
  }

  std::pair<bool, std::string> update_block_name(
      const model::Conn &c, const std::string &schema, const std::string &id, model::BlockType type, const std::string &name)
  {
    try {
      pool::Connection  pgPool = pool::acquire(c);
      pqxx::connection &pg     = *pgPool;
      pqxx::work        txn(pg);
      const std::string detail = type == model::BlockType::Field ? ".unit_b_field" : ".unit_b_method";
      txn.exec("UPDATE " + pg.quote_name(schema) + detail + " SET name = $1 WHERE id = $2", pqxx::params{txn, name, id});
      txn.commit();
      return {true, ""};
    } catch (const pqxx::sql_error &e) {
      return {false, sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {false, e.what()};
    }
  }

  std::pair<bool, std::string> delete_block(const model::Conn &c, const std::string &schema, const std::string &id, model::BlockType type)
  {
    try {
      pool::Connection  pgPool = pool::acquire(c);
      pqxx::connection &pg     = *pgPool;
      pqxx::work        txn(pg);
      const std::string qs     = pg.quote_name(schema);
      const std::string detail = type == model::BlockType::Field ? ".unit_b_field" : ".unit_b_method";
      if (type == model::BlockType::Method) txn.exec("DELETE FROM " + qs + ".unit_b_method_arg WHERE owner_method_id = $1", pqxx::params{txn, id});
      txn.exec("DELETE FROM " + qs + detail + " WHERE id = $1", pqxx::params{txn, id});
      txn.exec("DELETE FROM " + qs + ".unit_b WHERE id = $1", pqxx::params{txn, id});
      txn.commit();
      return {true, ""};
    } catch (const pqxx::sql_error &e) {
      return {false, sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {false, e.what()};
    }
  }

  std::pair<bool, std::string> update_block_size(const model::Conn &c, const std::string &schema, const std::string &id, float width, float height)
  {
    try {
      pool::Connection  pgPool = pool::acquire(c);
      pqxx::connection &pg     = *pgPool;
      pqxx::work        txn(pg);
      txn.exec("UPDATE " + pg.quote_name(schema) + ".unit_b SET width = $1, height = $2 WHERE id = $3", pqxx::params{txn, width, height, id});
      txn.commit();
      return {true, ""};
    } catch (const pqxx::sql_error &e) {
      return {false, sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {false, e.what()};
    }
  }

  std::pair<bool, std::string> update_block_position(const model::Conn &c, const std::string &schema, const std::string &id, float x, float y)
  {
    try {
      pool::Connection  pgPool = pool::acquire(c);
      pqxx::connection &pg     = *pgPool;
      pqxx::work        txn(pg);
      const std::string qs = pg.quote_name(schema);
      txn.exec("UPDATE " + qs + ".unit_b SET x = $1, y = $2 WHERE id = $3", pqxx::params{txn, x, y, id});

      // The block moved: drag its field's expression along (world coords =
      // block.{x,y} + the field's relative slot). Soft/optional tables.
      if (hasTable(txn, schema, "unit_e") && hasTable(txn, schema, "unit_b_field"))
        txn.exec("UPDATE " + qs +
                     ".unit_e e SET x = $1 + COALESCE(f.expr_x, 0), y = $2 + COALESCE(f.expr_y, 0) "
                     "FROM " +
                     qs + ".unit_b_field f WHERE f.id = $3 AND e.id = f.expr_id",
                 pqxx::params{txn, x, y, id});

      txn.commit();
      return {true, ""};
    } catch (const pqxx::sql_error &e) {
      return {false, sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {false, e.what()};
    }
  }

} // namespace back
