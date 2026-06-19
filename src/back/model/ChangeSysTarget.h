//
// Created by pompei on 2026-06-19.
//

#pragma once
#include <string>

namespace back::model {

  /**
   * Цель обслуживаемая системой изменений
   */
  struct ChangeSysTarget
  {
    /**
     * Идентификатор цели обслуживаемой системой изменений.
     *
     * Соответствует табличной колонке: undo_buffer.target_id
     */
    std::string targetId;

    /**
     * Тип цели обслуживаемой системой изменений.
     *
     * Соответствует табличной колонке: undo_buffer.target_type
     */
    std::string targetType;
  };

} // namespace back::model