#include "back/service/ExprServiceRW.h"
#include "back/etc/CustomId.h"
#include "back/etc/InitDb.h"
#include "back/pool/PoolService.h"
#include "back/service/RepoServiceR.h" // make_cs, sql_err_msg

namespace back {

  // Placeholder geometry for a freshly created unit_e: the expression is drawn
  // inline next to its field for now, so its own canvas box is unused. Avoid a
  // degenerate (zero-size) envelope for the generated geom column.
  static constexpr float EXPR_W = 1.f, EXPR_H = 1.f;

  // Return the field's expression unit_e id, creating the row with `type` if the
  // soft reference unit_b_field.expr_id is absent (NULL or dangling). Otherwise
  // updates the existing row's type. Runs inside the caller's transaction.
  static std::string ensure_field_expr(pqxx::work &txn, const std::string &qs, const std::string &field_id, model::ExprType type)
  {
    const std::string type_str = model::to_string(type);

    // Resolve the current soft reference: existing unit_e id, or empty.
    pqxx::result r = txn.exec("SELECT e.id "
                              "FROM " +
                                  qs +
                                  ".unit_b_field f "
                                  "JOIN " +
                                  qs +
                                  ".unit_e e ON e.id = f.expr_id "
                                  "WHERE f.id = $1",
                              pqxx::params{txn, field_id});
    if (!r.empty() && !r[0][0].is_null()) {
      const std::string eid = r[0][0].c_str();
      txn.exec("UPDATE " + qs + ".unit_e SET type = $1 WHERE id = $2", pqxx::params{txn, type_str, eid});
      return eid;
    }

    // None yet: create the unit_e row and point the field at it.
    const std::string eid = new_id();
    txn.exec("INSERT INTO " + qs + ".unit_e (id, type, x, y, width, height) VALUES ($1, $2, 0, 0, $3, $4)",
             pqxx::params{txn, eid, type_str, EXPR_W, EXPR_H});
    txn.exec("UPDATE " + qs + ".unit_b_field SET expr_id = $1 WHERE id = $2", pqxx::params{txn, eid, field_id});
    return eid;
  }

  std::pair<bool, std::string> set_field_expr_this_type(const model::Conn &c,
                                                        const std::string &schema,
                                                        const std::string &field_id,
                                                        model::ExprType    type)
  {
    try {
      pool::Connection  pgPool = pool::acquire(c);
      pqxx::connection &pg     = *pgPool;
      pqxx::work        txn(pg);
      InitDb(txn, pg, schema).init_unit_e_tables();

      ensure_field_expr(txn, pg.quote_name(schema), field_id, type);

      txn.commit();
      return {true, ""};
    } catch (const pqxx::sql_error &e) {
      return {false, sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {false, e.what()};
    }
  }

  std::pair<bool, std::string> set_field_expr_unit(const model::Conn &c,
                                                   const std::string &schema,
                                                   const std::string &field_id,
                                                   const std::string &unit_id)
  {
    try {
      pool::Connection  pgPool = pool::acquire(c);
      pqxx::connection &pg     = *pgPool;
      pqxx::work        txn(pg);
      const std::string qs = pg.quote_name(schema);
      InitDb(txn, pg, schema).init_unit_e_tables();

      const std::string eid = ensure_field_expr(txn, qs, field_id, model::ExprType::Unit);

      // unit_e_unit.id -> unit_e.id (1:1). Upsert the detail row.
      txn.exec("INSERT INTO " + qs +
                   ".unit_e_unit (id, unit_id) VALUES ($1, $2) "
                   "ON CONFLICT (id) DO UPDATE SET unit_id = EXCLUDED.unit_id",
               pqxx::params{txn, eid, unit_id});

      txn.commit();
      return {true, ""};
    } catch (const pqxx::sql_error &e) {
      return {false, sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {false, e.what()};
    }
  }

  std::pair<bool, std::string> update_field_expr_rect(
      const model::Conn &c, const std::string &schema, const std::string &field_id, float ex, float ey, float ew, float eh)
  {
    try {
      pool::Connection  pgPool = pool::acquire(c);
      pqxx::connection &pg     = *pgPool;
      pqxx::work        txn(pg);
      const std::string qs = pg.quote_name(schema);

      txn.exec("UPDATE " + qs + ".unit_b_field SET expr_x = $1, expr_y = $2, expr_width = $3, expr_height = $4 WHERE id = $5",
               pqxx::params{txn, ex, ey, ew, eh, field_id});

      // Keep the expression's own world rectangle in sync (only if it exists).
      txn.exec("UPDATE " + qs +
                   ".unit_e e SET x = b.x + $1, y = b.y + $2, width = $3, height = $4 "
                   "FROM " +
                   qs + ".unit_b b JOIN " + qs +
                   ".unit_b_field f ON f.id = b.id "
                   "WHERE b.id = $5 AND e.id = f.expr_id",
               pqxx::params{txn, ex, ey, ew, eh, field_id});

      txn.commit();
      return {true, ""};
    } catch (const pqxx::sql_error &e) {
      return {false, sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {false, e.what()};
    }
  }

} // namespace back
