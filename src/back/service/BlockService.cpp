#include "back/service/BlockService.h"
#include "back/etc/CustomId.h"
#include "back/etc/InitDb.h"
#include "back/etc/UtilDb.h"          // hasTable
#include "back/service/RepoService.h" // make_cs, sql_err_msg

namespace back {

  using namespace model;

  std::pair<std::vector<Block>, std::string> load_blocks_in_view(
      const Conn &c, const std::string &schema, const std::string &unit_id, float min_x, float min_y, float max_x, float max_y)
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

      std::vector<Block> out;
      out.reserve(rows.size());
      for (const auto &row : rows) {
        Block s{};
        s.id           = row[0].c_str();
        s.unit_id      = row[1].c_str();
        s.type         = block_type_from_string(row[2].c_str());
        s.x            = row[3].as<float>();
        s.y            = row[4].as<float>();
        s.width        = row[5].as<float>();
        s.height       = row[6].as<float>();
        s.name         = row[7].c_str();
        s.disabled     = row[8].as<bool>();
        s.method_type  = method_type_from_string(row[9].c_str());
        s.access       = method_access_from_string(row[10].c_str());
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
          for (Block &b : out) {
            if (b.id != owner) continue;
            MethodArg a{};
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
          for (Block &b : out) {
            if (b.id != bid) continue;
            b.expr_present = true;
            b.expr_type    = expr_type_from_string(row[1].c_str());
            // Resolve the unit name through the soft reference unit_e_unit.unit_id -> unit.id.
            if (b.expr_type == ExprType::Unit && has_unit && !row[2].is_null()) {
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

  std::pair<std::optional<BBox>, std::string> block_bbox_for_unit(const Conn &c, const std::string &schema, const std::string &unit_id)
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

      BBox b{r[0][0].as<float>(), r[0][1].as<float>(), r[0][2].as<float>(), r[0][3].as<float>()};
      return {b, ""};
    } catch (const pqxx::sql_error &e) {
      return {std::nullopt, sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {std::nullopt, e.what()};
    }
  }

  std::pair<std::string, std::string> create_block(const Conn        &c,
                                                   const std::string &schema,
                                                   const std::string &unit_id,
                                                   BlockType          type,
                                                   float              x,
                                                   float              y,
                                                   float              width,
                                                   float              height,
                                                   const std::string &name)
  {
    try {
      pqxx::connection pg(make_cs(c));
      pqxx::work       txn(pg);

      // Create the unit_b tables in case the repo predates this feature.
      InitDb(txn, pg, schema).init_unit_b_tables();

      const std::string qs       = pg.quote_name(schema);
      const std::string id       = new_id();
      const std::string type_str = to_string(type);

      txn.exec("INSERT INTO " + qs +
                   ".unit_b (id, unit_id, type, x, y, width, height) "
                   "VALUES ($1, $2, $3, $4, $5, $6, $7)",
               pqxx::params{txn, id, unit_id, type_str, x, y, width, height});

      const std::string detail = type == BlockType::Field ? ".unit_b_field" : ".unit_b_method";
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
      const Conn &c, const std::string &schema, const std::string &owner_method_id, double order_index, const std::string &name)
  {
    try {
      pqxx::connection pg(make_cs(c));
      pqxx::work       txn(pg);

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

  std::pair<std::string, std::string> append_method_arg(const Conn        &c,
                                                        const std::string &schema,
                                                        const std::string &owner_method_id,
                                                        const std::string &name)
  {
    try {
      pqxx::connection pg(make_cs(c));
      pqxx::work       txn(pg);

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

  std::pair<bool, std::string> update_method_arg_name(const Conn &c, const std::string &schema, const std::string &id, const std::string &name)
  {
    try {
      pqxx::connection pg(make_cs(c));
      pqxx::work       txn(pg);
      txn.exec("UPDATE " + pg.quote_name(schema) + ".unit_b_method_arg SET name = $1 WHERE id = $2", pqxx::params{txn, name, id});
      txn.commit();
      return {true, ""};
    } catch (const pqxx::sql_error &e) {
      return {false, sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {false, e.what()};
    }
  }

  std::pair<bool, std::string> delete_method_arg(const Conn &c, const std::string &schema, const std::string &id)
  {
    try {
      pqxx::connection pg(make_cs(c));
      pqxx::work       txn(pg);
      txn.exec("DELETE FROM " + pg.quote_name(schema) + ".unit_b_method_arg WHERE id = $1", pqxx::params{txn, id});
      txn.commit();
      return {true, ""};
    } catch (const pqxx::sql_error &e) {
      return {false, sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {false, e.what()};
    }
  }

  std::pair<bool, std::string> reorder_method_args(const Conn                     &c,
                                                   const std::string              &schema,
                                                   const std::string              &owner_method_id,
                                                   const std::vector<std::string> &ordered_ids)
  {
    try {
      pqxx::connection pg(make_cs(c));
      pqxx::work       txn(pg);
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
      const Conn &c, const std::string &schema, const std::string &id, const std::string &column, const std::string &value)
  {
    try {
      pqxx::connection pg(make_cs(c));
      pqxx::work       txn(pg);
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

  std::pair<bool, std::string> update_block_disabled(const Conn &c, const std::string &schema, const std::string &id, bool disabled)
  {
    try {
      pqxx::connection  pg(make_cs(c));
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

  std::pair<bool, std::string> update_method_type(const Conn &c, const std::string &schema, const std::string &id, MethodType type)
  {
    return update_method_column(c, schema, id, "type", to_string(type));
  }

  std::pair<bool, std::string> update_method_access(const Conn &c, const std::string &schema, const std::string &id, MethodAccess access)
  {
    return update_method_column(c, schema, id, "access", to_string(access));
  }

  // Helper: UPDATE one unit_b_field column for a single field id.
  static std::pair<bool, std::string> update_field_column(
      const Conn &c, const std::string &schema, const std::string &id, const std::string &column, const std::string &value)
  {
    try {
      pqxx::connection pg(make_cs(c));
      pqxx::work       txn(pg);
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

  std::pair<bool, std::string> update_field_access(const Conn &c, const std::string &schema, const std::string &id, MethodAccess access)
  {
    return update_field_column(c, schema, id, "access", to_string(access));
  }

  std::pair<bool, std::string> update_field_expr_id_used(const Conn &c, const std::string &schema, const std::string &id, bool expr_id_used)
  {
    return update_field_column(c, schema, id, "expr_id_used", expr_id_used ? "true" : "false");
  }

  std::pair<bool, std::string> update_field_size_bytes(const Conn &c, const std::string &schema, const std::string &id, int size_bytes)
  {
    return update_field_column(c, schema, id, "size_bytes", std::to_string(size_bytes));
  }

  std::pair<bool, std::string> update_block_name(
      const Conn &c, const std::string &schema, const std::string &id, BlockType type, const std::string &name)
  {
    try {
      pqxx::connection  pg(make_cs(c));
      pqxx::work        txn(pg);
      const std::string detail = type == BlockType::Field ? ".unit_b_field" : ".unit_b_method";
      txn.exec("UPDATE " + pg.quote_name(schema) + detail + " SET name = $1 WHERE id = $2", pqxx::params{txn, name, id});
      txn.commit();
      return {true, ""};
    } catch (const pqxx::sql_error &e) {
      return {false, sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {false, e.what()};
    }
  }

  std::pair<bool, std::string> delete_block(const Conn &c, const std::string &schema, const std::string &id, BlockType type)
  {
    try {
      pqxx::connection  pg(make_cs(c));
      pqxx::work        txn(pg);
      const std::string qs     = pg.quote_name(schema);
      const std::string detail = type == BlockType::Field ? ".unit_b_field" : ".unit_b_method";
      if (type == BlockType::Method) txn.exec("DELETE FROM " + qs + ".unit_b_method_arg WHERE owner_method_id = $1", pqxx::params{txn, id});
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

  std::pair<bool, std::string> update_block_size(const Conn &c, const std::string &schema, const std::string &id, float width, float height)
  {
    try {
      pqxx::connection pg(make_cs(c));
      pqxx::work       txn(pg);
      txn.exec("UPDATE " + pg.quote_name(schema) + ".unit_b SET width = $1, height = $2 WHERE id = $3", pqxx::params{txn, width, height, id});
      txn.commit();
      return {true, ""};
    } catch (const pqxx::sql_error &e) {
      return {false, sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {false, e.what()};
    }
  }

  std::pair<bool, std::string> update_block_position(const Conn &c, const std::string &schema, const std::string &id, float x, float y)
  {
    try {
      pqxx::connection  pg(make_cs(c));
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
