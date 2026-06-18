//
// Created by pompei on 2026-06-18.
//

#include "UndoService.h"

#include <optional>
#include <stdexcept>

namespace back {
  using model::UndoRowChange;

  UndoService::UndoService(pqxx::work &txn, pqxx::connection &pg, const std::string &schema)
      : txn_(txn)
      , pg_(pg)
      , schema_(schema)
  {}

  std::vector<UndoRowChange> UndoService::collectUndoChanges(std::vector<UndoRowChange> userChanges) const
  {
    std::vector<UndoRowChange> result;

    // Отменяем в обратном порядке: последнее исходное изменение откатывается
    // первым. Состояние читается ДО применения userChanges.
    for (auto it = userChanges.rbegin(); it != userChanges.rend(); ++it) {
      const UndoRowChange &u      = *it;
      const std::string    qtable = pg_.quote_name(schema_) + "." + pg_.quote_name(u.tableName);

      if (u.toDelete) {
        // Модель UndoRowChange умеет лишь удалять строки или менять значение
        // одной колонки — вставлять строки она не может, поэтому восстановить
        // удалённую строку нечем.
        throw std::runtime_error("UndoService: отмена удаления (toDelete=TRUE) не поддерживается "
                                 "текущей моделью UndoRowChange (нет вставки строк), таблица " +
                                 u.tableName);
      }

      // Отмена обновления строки (id == idValue).
      pqxx::result rows =
          txn_.exec_params("SELECT " + pg_.quote_name(u.colName) + " FROM " + qtable + " WHERE id = $1", u.idValue);

      UndoRowChange undo;
      undo.tableName = u.tableName;
      undo.idValue   = u.idValue;
      if (rows.empty()) {
        // Строки ещё нет: применение изменения её создаст, поэтому отменой
        // будет удаление этой строки.
        undo.toDelete = true;
      } else {
        // Строка есть: вернуть прежнее значение колонки colName.
        // NULL хранится как отсутствующее значение (nullopt); иначе — текст.
        undo.toDelete = false;
        undo.colName  = u.colName;
        undo.value    = rows[0][0].is_null() ? std::nullopt : std::optional<std::string>(rows[0][0].c_str());
      }
      result.push_back(std::move(undo));
    }

    return result;
  }
} // namespace back