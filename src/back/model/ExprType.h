//
// Created by pompei on 2026-06-18.
//

#pragma once
#include <string>

namespace back::model {

  /**
   * Тип выражения. Соответствует полю таблицы `unit_e.type`.
   *
   * В этом поле храниться не цифра, а текст равный наименованию переменной для этого перечисления.
   */
  enum class ExprType {

    /**
     * Указывает на текущий объект. Полностью храниться в таблице `unit_e`.
     *
     * Название: Этот Объект
     */
    ThisObject = 1,

    /**
     * Указывает на текущий юнит. Полностью храниться в таблице `unit_e`.
     *
     * Название: Этот Юнит
     */
    ThisUnit = 2,

    /**
     * Указывает на текущий метод. Полностью храниться в таблице `unit_e`.
     *
     * Название: Этот Метод
     */
    ThisMethod = 3,

    /**
     * Указывает на сторонний юнит.
     *
     * Храниться в таблицах `unit_e`, `unit_e_unit`.
     *
     * `unit_e_unit.unit_id` идентификатор юнита, на который указывает данное выражение.
     *
     * Название: Юнит
     */
    Unit = 4,

  };

  // The stored text is the enum variable's name (see unit_e.type).
  inline const char *to_string(ExprType t)
  {
    switch (t) {
      case ExprType::ThisObject: return "ThisObject";
      case ExprType::ThisUnit: return "ThisUnit";
      case ExprType::ThisMethod: return "ThisMethod";
      case ExprType::Unit: return "Unit";
    }
    return "ThisObject";
  }

  inline ExprType expr_type_from_string(const std::string &s)
  {
    if (s == "ThisUnit") return ExprType::ThisUnit;
    if (s == "ThisMethod") return ExprType::ThisMethod;
    if (s == "Unit") return ExprType::Unit;
    return ExprType::ThisObject;
  }

} // namespace back::model