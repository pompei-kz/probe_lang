#pragma once
#include "model/BBox.h"
#include "model/Conn.h"
#include "model/Block.h"
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace back {

  // Load the blocks of one unit whose geometry intersects the given world
  // rectangle. Uses the GIST index on unit_b.geom (the `&&` bbox operator).
  // Returns an empty list (no error) when the unit_b table is absent.
  std::pair<std::vector<model::Block>, std::string> load_blocks_in_view(
      const model::Conn &c, const std::string &schema, const std::string &unit_id, float min_x, float min_y, float max_x, float max_y);

  // Bounding box covering all blocks of one unit, or nullopt if it has none.
  // Used to center the editor camera when it first opens.
  std::pair<std::optional<model::BBox>, std::string> block_bbox_for_unit(const model::Conn &c, const std::string &schema, const std::string &unit_id);

  // Create a block: a unit_b row plus its unit_b_method / unit_b_field
  // detail row. Returns the new id on success (first element empty on failure).
  std::pair<std::string, std::string> create_block(const model::Conn   &c,
                                                       const std::string   &schema,
                                                       const std::string   &unit_id,
                                                       model::BlockType type,
                                                       float                x,
                                                       float                y,
                                                       float                width,
                                                       float                height,
                                                       const std::string   &name);

  // Update a block's name in its detail table (chosen by `type`).
  std::pair<bool, std::string> update_block_name(
      const model::Conn &c, const std::string &schema, const std::string &id, model::BlockType type, const std::string &name);

  // Create one argument of a method block (a unit_b_method_arg row).
  // Returns the new id on success (first element empty on failure).
  std::pair<std::string, std::string> create_method_arg(
      const model::Conn &c, const std::string &schema, const std::string &owner_method_id, double order_index, const std::string &name);

  // Create one argument and append it at the end of the method: its order_index
  // is max(order_index in this method) + 1, computed atomically so a prior
  // delete can't make it land in the middle. Returns the new id (empty on error).
  std::pair<std::string, std::string> append_method_arg(
      const model::Conn &c, const std::string &schema, const std::string &owner_method_id, const std::string &name);

  // Update a method argument's name (unit_b_method_arg.name).
  std::pair<bool, std::string> update_method_arg_name(
      const model::Conn &c, const std::string &schema, const std::string &id, const std::string &name);

  // Delete one method argument (unit_b_method_arg row) by id.
  std::pair<bool, std::string> delete_method_arg(const model::Conn &c, const std::string &schema, const std::string &id);

  // Rewrite the order of a method's arguments: order_index becomes each id's
  // position in `ordered_ids` (0..n-1). Used by drag-to-reorder.
  std::pair<bool, std::string> reorder_method_args(
      const model::Conn &c, const std::string &schema, const std::string &owner_method_id, const std::vector<std::string> &ordered_ids);

  // Update a block's disabled flag (unit_b.disabled — common to methods and fields).
  std::pair<bool, std::string> update_block_disabled(const model::Conn &c, const std::string &schema, const std::string &id, bool disabled);

  // Update method-only attributes (unit_b_method.type / access).
  std::pair<bool, std::string> update_method_type(const model::Conn &c, const std::string &schema, const std::string &id, model::MethodType type);
  std::pair<bool, std::string> update_method_access(const model::Conn &c, const std::string &schema, const std::string &id, model::MethodAccess access);

  // Update field attributes (unit_b_field.access).
  std::pair<bool, std::string> update_field_access(const model::Conn &c, const std::string &schema, const std::string &id, model::MethodAccess access);

  // Delete a block: its unit_b row, its detail row (unit_b_method /
  // unit_b_field per `type`) and, for methods, all of its arguments.
  std::pair<bool, std::string> delete_block(const model::Conn &c, const std::string &schema, const std::string &id, model::BlockType type);

  // Update a block's box size (unit_b.width / height). The geom column is
  // regenerated automatically.
  std::pair<bool, std::string> update_block_size(
      const model::Conn &c, const std::string &schema, const std::string &id, float width, float height);

  // Update a block's top-left position (unit_b.x / y). The geom column is
  // regenerated automatically.
  std::pair<bool, std::string> update_block_position(const model::Conn &c, const std::string &schema, const std::string &id, float x, float y);

} // namespace back
