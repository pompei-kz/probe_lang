#pragma once
#include "model/Conn.h"
#include "model/Statement.h"
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace back {

  // Axis-aligned bounding box in world coordinates.
  struct BBox
  {
    float min_x, min_y, max_x, max_y;
  };

  // Load the statements of one unit whose geometry intersects the given world
  // rectangle. Uses the GIST index on unit_st.geom (the `&&` bbox operator).
  // Returns an empty list (no error) when the unit_st table is absent.
  std::pair<std::vector<model::Statement>, std::string> load_statements_in_view(
      const model::Conn &c, const std::string &schema, const std::string &unit_id, float min_x, float min_y, float max_x, float max_y);

  // Bounding box covering all statements of one unit, or nullopt if it has none.
  // Used to centre the editor camera when it first opens.
  std::pair<std::optional<BBox>, std::string> statement_bbox_for_unit(const model::Conn &c, const std::string &schema, const std::string &unit_id);

  // Create a statement: a unit_st row plus its unit_st_method / unit_st_field
  // detail row. Returns the new id on success (first element empty on failure).
  std::pair<std::string, std::string> create_statement(const model::Conn  &c,
                                                        const std::string  &schema,
                                                        const std::string  &unit_id,
                                                        model::StatementType type,
                                                        float                x,
                                                        float                y,
                                                        float                width,
                                                        float                height,
                                                        const std::string  &name);

  // Update a statement's name in its detail table (chosen by `type`).
  std::pair<bool, std::string> update_statement_name(
      const model::Conn &c, const std::string &schema, const std::string &id, model::StatementType type, const std::string &name);

  // Update a statement's box size (unit_st.width / height). The geom column is
  // regenerated automatically.
  std::pair<bool, std::string> update_statement_size(
      const model::Conn &c, const std::string &schema, const std::string &id, float width, float height);

} // namespace back
