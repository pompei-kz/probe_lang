#include "ExprService.h"
#include "CustomId.h"
#include "InitDb.h"
#include "RepoService.h" // make_cs, sql_err_msg

namespace back {

  using namespace model;

  // Placeholder geometry for a freshly created unit_e: the expression is drawn
  // inline next to its field for now, so its own canvas box is unused. Avoid a
  // degenerate (zero-size) envelope for the generated geom column.
  static constexpr float EXPR_W = 1.f, EXPR_H = 1.f;

  // Return the field's expression unit_e id, creating the row with `type` if the
  // soft reference unit_b_field.expr_id is absent (NULL or dangling). Otherwise
  // updates the existing row's type. Runs inside the caller's transaction.
  static std::string ensure_field_expr(pqxx::work &txn, const std::string &qs, const std::string &field_id, ExprType type)
  {
    const std::string type_str = to_string(type);

    // Resolve the current soft reference: existing unit_e id, or empty.
    pqxx::result r = txn.exec_params("SELECT e.id "
                                     "FROM " + qs + ".unit_b_field f "
                                     "JOIN " + qs + ".unit_e e ON e.id = f.expr_id "
                                     "WHERE f.id = $1",
                                     field_id);
    if (!r.empty() && !r[0][0].is_null()) {
      const std::string eid = r[0][0].c_str();
      txn.exec_params("UPDATE " + qs + ".unit_e SET type = $1 WHERE id = $2", type_str, eid);
      return eid;
    }

    // None yet: create the unit_e row and point the field at it.
    const std::string eid = new_id();
    txn.exec_params("INSERT INTO " + qs + ".unit_e (id, type, x, y, width, height) VALUES ($1, $2, 0, 0, $3, $4)", eid, type_str, EXPR_W, EXPR_H);
    txn.exec_params("UPDATE " + qs + ".unit_b_field SET expr_id = $1 WHERE id = $2", eid, field_id);
    return eid;
  }

  std::pair<bool, std::string> set_field_expr_this_type(
      const Conn &c, const std::string &schema, const std::string &field_id, ExprType type)
  {
    try {
      pqxx::connection pg(make_cs(c));
      pqxx::work       txn(pg);
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

  std::pair<bool, std::string> set_field_expr_unit(
      const Conn &c, const std::string &schema, const std::string &field_id, const std::string &unit_id)
  {
    try {
      pqxx::connection  pg(make_cs(c));
      pqxx::work        txn(pg);
      const std::string qs = pg.quote_name(schema);
      InitDb(txn, pg, schema).init_unit_e_tables();

      const std::string eid = ensure_field_expr(txn, qs, field_id, ExprType::Unit);

      // unit_e_unit.id -> unit_e.id (1:1). Upsert the detail row.
      txn.exec_params("INSERT INTO " + qs + ".unit_e_unit (id, unit_id) VALUES ($1, $2) "
                      "ON CONFLICT (id) DO UPDATE SET unit_id = EXCLUDED.unit_id",
                      eid, unit_id);

      txn.commit();
      return {true, ""};
    } catch (const pqxx::sql_error &e) {
      return {false, sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {false, e.what()};
    }
  }

} // namespace back
