#pragma once
#include "back/model/ConnStore.h"
#include "back/model/Unit.h"
#include <string>
#include <utility>

namespace back {

  // Create a new unit. parent_folder_id empty = directly under the repository.
  std::pair<bool, std::string> create_unit(
      const model::ConnStore &c, const std::string &schema, const std::string &parent_folder_id, const std::string &name, model::UnitType type);

  // Edit an existing unit's name and type.
  std::pair<bool, std::string> edit_unit(
      const model::ConnStore &c, const std::string &schema, const std::string &id, const std::string &name, model::UnitType type);

  // Delete a unit by id.
  std::pair<bool, std::string> delete_unit(const model::ConnStore &c, const std::string &schema, const std::string &id);

  // Ensure the unit table exists in every repository schema of the connection.
  // Called when a connection switches to the "connected" state.
  std::pair<bool, std::string> ensure_unit_tables(const model::ConnStore &c);

} // namespace back
