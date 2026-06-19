//
// Created by pompei on 2026-06-18.
//

#include "ChangeSystem.h"

#include <optional>

namespace back {
  using model::RowChange;

  ChangeSystem::ChangeSystem(pqxx::work &txn, pqxx::connection &pg, const std::string &schema)
      : txn_(txn)
      , pg_(pg)
      , schema_(schema)
  {}

  std::vector<RowChange> ChangeSystem::collectUndoChanges(std::vector<RowChange> userChanges) const
  {
    std::vector<RowChange> result;

    // Отменяем в обратном порядке: последнее исходное изменение откатывается
    // первым. Состояние читается ДО применения userChanges.
    for (auto it = userChanges.rbegin(); it != userChanges.rend(); ++it) {
      const RowChange &u      = *it;
      const std::string    qtable = pg_.quote_name(schema_) + "." + pg_.quote_name(u.tableName);

      if (u.toDelete) {
        // Отмена удаления: вставки в модели нет, поэтому строку воссоздаём
        // набором обновлений — по одному изменению на каждую колонку текущей
        // строки. Первое применённое изменение создаст строку (upsert по id),
        // остальные проставят оставшиеся колонки.
        pqxx::result rows = txn_.exec_params("SELECT * FROM " + qtable + " WHERE id = $1", u.idValue);
        if (rows.empty()) continue; // строки нет — удаление ничего не сделает, отменять нечего

        for (const pqxx::field &f : rows[0]) {
          if (std::string(f.name()) == "id") continue; // id уже задан в idValue каждого изменения

          RowChange undo;
          undo.tableName = u.tableName;
          undo.idValue   = u.idValue;
          undo.toDelete  = false;
          undo.colName   = f.name();
          undo.value     = f.is_null() ? std::nullopt : std::optional<std::string>(f.c_str());
          result.push_back(std::move(undo));
        }
        continue;
      }

      // Отмена обновления строки (id == idValue).
      pqxx::result rows =
          txn_.exec_params("SELECT " + pg_.quote_name(u.colName) + " FROM " + qtable + " WHERE id = $1", u.idValue);

      RowChange undo;
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