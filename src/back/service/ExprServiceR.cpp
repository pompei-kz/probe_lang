#include "back/service/ExprServiceR.h"
#include "back/etc/UtilDb.h" // hasTable
#include "back/pool/PoolService.h"
#include "back/service/RepoServiceR.h" // make_cs, sql_err_msg

namespace back {

  std::pair<std::vector<model::Expr>, std::string> load_exprs_in_view(
      const model::Conn &c, const std::string &schema, const std::string &unit_id, float min_x, float min_y, float max_x, float max_y)
  {
    try {
      pool::Connection  pgPool = pool::acquire(c);
      pqxx::connection &pg     = *pgPool;
      pqxx::work        txn(pg);

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

      std::vector<model::Expr> out;
      out.reserve(rows.size());
      for (const auto &row : rows) {
        model::Expr e{};
        e.id             = row[0].c_str();
        e.type           = model::expr_type_from_string(row[1].c_str());
        e.x              = row[2].as<float>();
        e.y              = row[3].as<float>();
        e.width          = row[4].as<float>();
        e.height         = row[5].as<float>();
        e.owner_block_id = row[6].c_str();
        // Resolve the unit name through the soft reference unit_e_unit.unit_id -> unit.id.
        if (e.type == model::ExprType::Unit && has_unit && !row[7].is_null()) {
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

} // namespace back
