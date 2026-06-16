#include "BlockService.h"
#include "CustomId.h"
#include "InitDb.h"
#include "RepoService.h" // make_cs, sql_err_msg
#include "UtilDb.h"      // hasTable

namespace back {

  using namespace model;

  std::pair<std::vector<Block>, std::string> load_blocks_in_view(
      const Conn &c, const std::string &schema, const std::string &unit_id, float min_x, float min_y, float max_x, float max_y)
  {
    try {
      pqxx::connection pg(make_cs(c));
      pqxx::work       txn(pg);

      if (!hasTable(txn, schema, "unit_bl")) return {{}, ""};

      const std::string qs = pg.quote_name(schema);
      pqxx::result      rows =
          txn.exec_params("SELECT s.id, s.unit_id, s.type, s.x, s.y, s.width, s.height, "
                          "       COALESCE(m.name, f.name, '') "
                          "FROM " + qs + ".unit_bl s "
                          "LEFT JOIN " + qs + ".unit_bl_method m ON m.id = s.id "
                          "LEFT JOIN " + qs + ".unit_bl_field  f ON f.id = s.id "
                          "WHERE s.unit_id = $1 AND s.geom && ST_MakeEnvelope($2, $3, $4, $5, 0)",
                          unit_id,
                          min_x,
                          min_y,
                          max_x,
                          max_y);

      std::vector<Block> out;
      out.reserve(rows.size());
      for (const pqxx::row &row : rows) {
        Block s{};
        s.id      = row[0].c_str();
        s.unit_id = row[1].c_str();
        s.type    = block_type_from_string(row[2].c_str());
        s.x       = row[3].as<float>();
        s.y       = row[4].as<float>();
        s.width   = row[5].as<float>();
        s.height  = row[6].as<float>();
        s.name    = row[7].c_str();
        out.push_back(std::move(s));
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

      if (!hasTable(txn, schema, "unit_bl")) return {std::nullopt, ""};

      pqxx::result r = txn.exec_params("SELECT min(x), min(y), max(x + width), max(y + height) "
                                       "FROM " + pg.quote_name(schema) + ".unit_bl WHERE unit_id = $1",
                                       unit_id);
      if (r.empty() || r[0][0].is_null()) return {std::nullopt, ""};

      BBox b{r[0][0].as<float>(), r[0][1].as<float>(), r[0][2].as<float>(), r[0][3].as<float>()};
      return {b, ""};
    } catch (const pqxx::sql_error &e) {
      return {std::nullopt, sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {std::nullopt, e.what()};
    }
  }

  std::pair<std::string, std::string> create_block(const Conn         &c,
                                                       const std::string  &schema,
                                                       const std::string  &unit_id,
                                                       BlockType        type,
                                                       float                x,
                                                       float                y,
                                                       float                width,
                                                       float                height,
                                                       const std::string  &name)
  {
    try {
      pqxx::connection pg(make_cs(c));
      pqxx::work       txn(pg);

      // Create the unit_bl tables in case the repo predates this feature.
      init_unit_bl_tables(txn, pg, schema);

      const std::string qs       = pg.quote_name(schema);
      const std::string id       = new_id();
      const std::string type_str = to_string(type);

      txn.exec_params("INSERT INTO " + qs + ".unit_bl (id, unit_id, type, x, y, width, height) "
                      "VALUES ($1, $2, $3, $4, $5, $6, $7)",
                      id,
                      unit_id,
                      type_str,
                      x,
                      y,
                      width,
                      height);

      const std::string detail = type == BlockType::Field ? ".unit_bl_field" : ".unit_bl_method";
      txn.exec_params("INSERT INTO " + qs + detail + " (id, name) VALUES ($1, $2)", id, name);

      txn.commit();
      return {id, ""};
    } catch (const pqxx::sql_error &e) {
      return {"", sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {"", e.what()};
    }
  }

  std::pair<bool, std::string> update_block_name(
      const Conn &c, const std::string &schema, const std::string &id, BlockType type, const std::string &name)
  {
    try {
      pqxx::connection  pg(make_cs(c));
      pqxx::work        txn(pg);
      const std::string detail = type == BlockType::Field ? ".unit_bl_field" : ".unit_bl_method";
      txn.exec_params("UPDATE " + pg.quote_name(schema) + detail + " SET name = $1 WHERE id = $2", name, id);
      txn.commit();
      return {true, ""};
    } catch (const pqxx::sql_error &e) {
      return {false, sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {false, e.what()};
    }
  }

  std::pair<bool, std::string> update_block_size(
      const Conn &c, const std::string &schema, const std::string &id, float width, float height)
  {
    try {
      pqxx::connection pg(make_cs(c));
      pqxx::work       txn(pg);
      txn.exec_params("UPDATE " + pg.quote_name(schema) + ".unit_bl SET width = $1, height = $2 WHERE id = $3", width, height, id);
      txn.commit();
      return {true, ""};
    } catch (const pqxx::sql_error &e) {
      return {false, sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {false, e.what()};
    }
  }

  std::pair<bool, std::string> update_block_position(
      const Conn &c, const std::string &schema, const std::string &id, float x, float y)
  {
    try {
      pqxx::connection pg(make_cs(c));
      pqxx::work       txn(pg);
      txn.exec_params("UPDATE " + pg.quote_name(schema) + ".unit_bl SET x = $1, y = $2 WHERE id = $3", x, y, id);
      txn.commit();
      return {true, ""};
    } catch (const pqxx::sql_error &e) {
      return {false, sql_err_msg(e)};
    } catch (const std::exception &e) {
      return {false, e.what()};
    }
  }

} // namespace back
