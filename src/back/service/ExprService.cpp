#include "back/service/ExprService.h"
#include "back/etc/CustomId.h"
#include "back/etc/InitDb.h"
#include "back/etc/UtilDb.h"          // hasTable
#include "back/service/RepoService.h" // make_cs, sql_err_msg

namespace back {

  using namespace model;

  std::pair<std::vector<Expr>, std::string> load_exprs_in_view(
      const Conn &c, const std::string &schema, const std::string &unit_id, float min_x, float min_y, float max_x, float max_y)
  {
    try {
      pqxx::connection pg(make_cs(c));
      pqxx::work       txn(pg);

      if (!hasTable(txn, schema, "unit_e") || !hasTable(txn, schema, "unit_b_field") || !hasTable(txn, schema, "unit_b")) return {{}, ""};

      const std::string qs       = pg.quote_name(schema);
      const bool        has_unit = hasTable(txn, schema, "unit");
      const bool        has_eu   = hasTable(txn, schema, "unit_e_unit");

      // Expressions of this unit's fields whose world rectangle hits the viewport.
      pqxx::result rows =
          txn.exec("SELECT e.id, e.type, e.x, e.y, e.width, e.height, b.id" + std::string(has_eu ? ", eu.unit_id " : ", NULL ") + "FROM " + qs +
                       ".unit_e e "
                       "JOIN " +
                       qs +
                       ".unit_b_field f ON f.expr_id = e.id "
                       "JOIN " +
                       qs + ".unit_b b ON b.id = f.id " + (has_eu ? "LEFT JOIN " + qs + ".unit_e_unit eu ON eu.id = e.id " : "") +
                       "WHERE b.unit_id = $1 AND COALESCE(f.expr_id_used, false) = true "
                       "  AND e.geom && ST_MakeEnvelope($2, $3, $4, $5, 0)",
                   pqxx::params{txn, unit_id, min_x, min_y, max_x, max_y});

      std::vector<Expr> out;
      out.reserve(rows.size());
      for (const auto &row : rows) {
        Expr e{};
        e.id             = row[0].c_str();
        e.type           = expr_type_from_string(row[1].c_str());
        e.x              = row[2].as<float>();
        e.y              = row[3].as<float>();
        e.width          = row[4].as<float>();
        e.height         = row[5].as<float>();
        e.owner_block_id = row[6].c_str();
        // Resolve the unit name through the soft reference unit_e_unit.unit_id -> unit.id.
        if (e.type == ExprType::Unit && has_unit && !row[7].is_null()) {
          const std::string uid = row[7].c_str();
          pqxx::result      ur  = txn.exec("SELECT name FROM " + qs + ".unit WHERE id = $1", pqxx::params{txn, uid});
          if (!ur.empty()) {
            e.unit_present = true;
            e.unit_name    = ur[0][0].c_str();
          }
        }
        out.push_back(std::move(e));
      }
      return {std::move(out), ""};
    } catch (const pqxx::sql_error &e) {
      return {{}, sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {{}, e.what()};
    }
  }

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

  std::pair<bool, std::string> set_field_expr_this_type(const Conn &c, const std::string &schema, const std::string &field_id, ExprType type)
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

  std::pair<bool, std::string> set_field_expr_unit(const Conn &c, const std::string &schema, const std::string &field_id, const std::string &unit_id)
  {
    try {
      pqxx::connection  pg(make_cs(c));
      pqxx::work        txn(pg);
      const std::string qs = pg.quote_name(schema);
      InitDb(txn, pg, schema).init_unit_e_tables();

      const std::string eid = ensure_field_expr(txn, qs, field_id, ExprType::Unit);

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
      const Conn &c, const std::string &schema, const std::string &field_id, float ex, float ey, float ew, float eh)
  {
    try {
      pqxx::connection  pg(make_cs(c));
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
