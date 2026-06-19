#include "back/service/BlockServiceR.h"
#include "back/etc/UtilDb.h"           // hasTable
#include "back/service/RepoServiceR.h" // make_cs, sql_err_msg

namespace back {

  std::pair<std::vector<model::Block>, std::string> load_blocks_in_view(
      const model::ConnStore &c, const std::string &schema, const std::string &unit_id, float min_x, float min_y, float max_x, float max_y)
  {
    try {
      pqxx::connection pg(make_cs(c));
      pqxx::work       txn(pg);

      if (!hasTable(txn, schema, "unit_b")) return {{}, ""};

      const std::string qs   = pg.quote_name(schema);
      pqxx::result      rows = txn.exec("SELECT s.id, s.unit_id, s.type, s.x, s.y, s.width, s.height, "
                                        "       COALESCE(m.name, f.name, ''), "
                                        "       s.disabled, "
                                        "       COALESCE(m.type, 'Inner'), "
                                        "       COALESCE(m.access, f.access, 'Private'), "
                                        "       COALESCE(f.expr_id_used, false), "
                                        "       COALESCE(f.size_bytes, 0), "
                                        "       f.expr_x, f.expr_y, f.expr_width, f.expr_height "
                                        "FROM " +
                                            qs +
                                            ".unit_b s "
                                            "LEFT JOIN " +
                                            qs +
                                            ".unit_b_method m ON m.id = s.id "
                                            "LEFT JOIN " +
                                            qs +
                                            ".unit_b_field  f ON f.id = s.id "
                                            "WHERE s.unit_id = $1 AND s.geom && ST_MakeEnvelope($2, $3, $4, $5, 0)",
                                        pqxx::params{txn, unit_id, min_x, min_y, max_x, max_y});

      std::vector<model::Block> out;
      out.reserve(rows.size());
      for (const auto &row : rows) {
        model::Block s{};
        s.id           = row[0].c_str();
        s.unit_id      = row[1].c_str();
        s.type         = model::block_type_from_string(row[2].c_str());
        s.x            = row[3].as<float>();
        s.y            = row[4].as<float>();
        s.width        = row[5].as<float>();
        s.height       = row[6].as<float>();
        s.name         = row[7].c_str();
        s.disabled     = row[8].as<bool>();
        s.method_type  = model::method_type_from_string(row[9].c_str());
        s.access       = model::method_access_from_string(row[10].c_str());
        s.expr_id_used = row[11].as<bool>();
        s.size_bytes   = row[12].as<int>();
        if (!row[15].is_null()) { // expr_width present → a slot has been stored
          s.expr_has_rect = true;
          s.expr_x        = row[13].is_null() ? 0.f : row[13].as<float>();
          s.expr_y        = row[14].is_null() ? 0.f : row[14].as<float>();
          s.expr_width    = row[15].as<float>();
          s.expr_height   = row[16].is_null() ? 0.f : row[16].as<float>();
        }
        out.push_back(std::move(s));
      }

      // Attach method arguments. One query for the whole unit (joined through
      // unit_b), then distributed to the in-view method blocks by owner id.
      if (hasTable(txn, schema, "unit_b_method_arg")) {
        pqxx::result args = txn.exec("SELECT a.id, a.owner_method_id, a.name "
                                     "FROM " +
                                         qs +
                                         ".unit_b_method_arg a "
                                         "JOIN " +
                                         qs +
                                         ".unit_b s ON s.id = a.owner_method_id "
                                         "WHERE s.unit_id = $1 "
                                         "ORDER BY a.owner_method_id, a.order_index, a.id",
                                     pqxx::params{txn, unit_id});
        for (const auto &row : args) {
          const std::string owner = row[1].c_str();
          for (model::Block &b : out) {
            if (b.id != owner) continue;
            model::MethodArg a{};
            a.id   = row[0].c_str();
            a.name = row[2].c_str();
            b.args.push_back(std::move(a));
            break;
          }
        }
      }

      // Attach expression render data to fields. Soft references throughout: a
      // field's expr_id may be NULL or dangling (→ no row, expr absent), and a
      // Unit expression's unit_id may be NULL or dangling (→ unit unresolved).
      if (hasTable(txn, schema, "unit_e") && hasTable(txn, schema, "unit_e_unit")) {
        // The `unit` table may be absent (older repo); a Unit expression then
        // simply has an unresolved reference — not an error (soft coupling).
        const bool   has_unit = hasTable(txn, schema, "unit");
        pqxx::result exprs    = txn.exec("SELECT s.id, e.type, eu.unit_id "
                                         "FROM " +
                                             qs +
                                             ".unit_b s "
                                             "JOIN " +
                                             qs +
                                             ".unit_b_field f ON f.id = s.id "
                                             "JOIN " +
                                             qs +
                                             ".unit_e e ON e.id = f.expr_id "
                                             "LEFT JOIN " +
                                             qs +
                                             ".unit_e_unit eu ON eu.id = e.id "
                                             "WHERE s.unit_id = $1",
                                         pqxx::params{txn, unit_id});
        for (const auto &row : exprs) {
          const std::string bid = row[0].c_str();
          for (model::Block &b : out) {
            if (b.id != bid) continue;
            b.expr_present = true;
            b.expr_type    = model::expr_type_from_string(row[1].c_str());
            // Resolve the unit name through the soft reference unit_e_unit.unit_id -> unit.id.
            if (b.expr_type == model::ExprType::Unit && has_unit && !row[2].is_null()) {
              const std::string uid = row[2].c_str();
              pqxx::result      ur  = txn.exec("SELECT name FROM " + qs + ".unit WHERE id = $1", pqxx::params{txn, uid});
              if (!ur.empty()) {
                b.expr_unit_present = true;
                b.expr_unit_name    = ur[0][0].c_str();
              }
            }
            break;
          }
        }
      }
      return {std::move(out), ""};
    } catch (const pqxx::sql_error &e) {
      return {{}, sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {{}, e.what()};
    }
  }

  std::pair<std::optional<model::BBox>, std::string> block_bbox_for_unit(const model::ConnStore &c, const std::string &schema, const std::string &unit_id)
  {
    try {
      pqxx::connection pg(make_cs(c));
      pqxx::work       txn(pg);

      if (!hasTable(txn, schema, "unit_b")) return {std::nullopt, ""};

      pqxx::result r = txn.exec("SELECT min(x), min(y), max(x + width), max(y + height) "
                                "FROM " +
                                    pg.quote_name(schema) + ".unit_b WHERE unit_id = $1",
                                pqxx::params{txn, unit_id});
      if (r.empty() || r[0][0].is_null()) return {std::nullopt, ""};

      model::BBox b{r[0][0].as<float>(), r[0][1].as<float>(), r[0][2].as<float>(), r[0][3].as<float>()};
      return {b, ""};
    } catch (const pqxx::sql_error &e) {
      return {std::nullopt, sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {std::nullopt, e.what()};
    }
  }

} // namespace back
