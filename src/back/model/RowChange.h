//
// Created by pompei on 2026-06-16.
//

#pragma once
#include <string>

namespace back::model {

  /**
   * Указывает на изменение одной колонки в таблице или удаление строки строки целиком
   */
  struct RowChange
  {
    /**
     * Имя таблицы, запись которой нужно удалить или изменить
     */
    std::string tableName;

    /**
     * Идентификатор строки
     */
    std::string id;

    /**
     * Если true, то эту строку нужно удалить. Остальные параметры не используются.
     * Если false, то нужно изменить колонку в строке с ИД=this->id, имя колонки this->colName и в неё установить значение this->value
     */
    bool toDelete;

    /**
     * Имя колонки, которую нужно изменить, если this->toDelete == false
     */
    std::string colName;

    /**
     * Значение, которое нужно присвоить колонке.
     * Колонка может иметь другой тип.
     */
    std::string value;
  };

} // namespace back::model