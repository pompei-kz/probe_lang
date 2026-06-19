//
// Created by pompei on 2026-06-18.
//

#include "ChangeSystem.h"

#include "CustomId.h"
#include "model/ChangeOp.h"

#include <optional>

namespace back {
  using model::RowChange;

  namespace {

    // Применяет одно изменение строки к реальной таблице данных.
    // toDelete == true  → строка `id == idValue` удаляется.
    // toDelete == false → upsert по `id`: строка создаётся, если её ещё нет,
    //                     иначе у неё проставляется колонка colName. NULL хранится
    //                     как отсутствующее значение (nullopt).
    void applyRowChange(pqxx::work &txn, pqxx::connection &pg, const std::string &schema, const RowChange &c)
    {
      const std::string qtable = pg.quote_name(schema) + "." + pg.quote_name(c.tableName);

      if (c.toDelete) {
        txn.exec("DELETE FROM " + qtable + " WHERE id = $1", pqxx::params{txn, c.idValue});
        return;
      }

      const std::string qcol = pg.quote_name(c.colName);
      txn.exec("INSERT INTO " + qtable + " (id, " + qcol +
                   ") VALUES ($1, $2) " //
                   "ON CONFLICT (id) DO UPDATE SET " +
                   qcol + " = EXCLUDED." + qcol,
               pqxx::params{txn, c.idValue, c.value});
    }

  } // namespace

  ChangeSystem::ChangeSystem(pqxx::work &txn, pqxx::connection &pg, const std::string &schema)
      : txn_(txn)
      , pg_(pg)
      , schema_(schema)
  {}

  std::vector<RowChange> ChangeSystem::collectUndoChanges(const std::vector<RowChange> &userChanges) const
  {
    std::vector<RowChange> result;

    // Отменяем в обратном порядке: последнее исходное изменение откатывается
    // первым. Состояние читается ДО применения userChanges.
    for (auto it = userChanges.rbegin(); it != userChanges.rend(); ++it) {
      const RowChange  &u      = *it;
      const std::string qtable = pg_.quote_name(schema_) + "." + pg_.quote_name(u.tableName);

      if (u.toDelete) {
        // Отмена удаления: вставки в модели нет, поэтому строку воссоздаём
        // набором обновлений — по одному изменению на каждую колонку текущей
        // строки. Первое применённое изменение создаст строку (upsert по id),
        // остальные проставят оставшиеся колонки.
        pqxx::result rows = txn_.exec("SELECT * FROM " + qtable + " WHERE id = $1", pqxx::params{txn_, u.idValue});
        if (rows.empty()) continue; // строки нет — удаление ничего не сделает, отменять нечего

        for (const auto &f : rows[0]) {
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
      pqxx::result rows = txn_.exec("SELECT " + pg_.quote_name(u.colName) + " FROM " + qtable + " WHERE id = $1", pqxx::params{txn_, u.idValue});

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

  void ChangeSystem::apply(const std::vector<RowChange> &userChanges, const model::ChangeOp &operation, const model::ChangeSysTarget &target) const
  {
    const std::string sq = pg_.quote_name(schema_);

    // 1. Найти буфер отмены для цели; если его ещё нет — создать.
    std::string bufferId;
    {
      pqxx::result r = txn_.exec("SELECT id FROM " + sq + ".undo_buffer WHERE target_id = $1 AND target_type = $2",
                                 pqxx::params{txn_, target.targetId, target.targetType});
      if (r.empty()) {
        bufferId = new_id();
        txn_.exec("INSERT INTO " + sq + ".undo_buffer (id, target_id, target_type) VALUES ($1, $2, $3)",
                  pqxx::params{txn_, bufferId, target.targetId, target.targetType});
      } else {
        bufferId = r[0][0].c_str();
      }
    }

    // 2. Стереть redo: новое действие делает ранее отменённые операции
    //    недоступными для повтора, поэтому удаляем их вместе с изменениями.
    txn_.exec("DELETE FROM " + sq +
                  ".undo_row_change WHERE undo_op_id IN (" //
                  "SELECT id FROM " +
                  sq + ".undo_op WHERE undo_buffer_id = $1 AND undone = TRUE)",
              pqxx::params{txn_, bufferId});
    txn_.exec("DELETE FROM " + sq + ".undo_op WHERE undo_buffer_id = $1 AND undone = TRUE", pqxx::params{txn_, bufferId});

    // 3. Собрать отменяющие изменения ДО применения userChanges: они читают
    //    текущее (старое) состояние строк из БД.
    std::vector<RowChange> undoChanges = collectUndoChanges(userChanges);

    // 4. Создать новую операцию в буфере. Её order_index выдаётся
    //    последовательностью, а undone = FALSE делает её активной операцией
    //    буфера (последняя с undone = FALSE — та, которую можно отменить).
    const std::string opId = new_id();
    txn_.exec("INSERT INTO " + sq +
                  ".undo_op (id, undo_buffer_id, undone, group_name, name)"
                  " VALUES ($1, $2, FALSE, $3, $4)",
              pqxx::params{txn_, opId, bufferId, operation.group, operation.operation});

    // 5. Сохранить изменения операции: Forward — что сделал пользователь,
    //    ForUndo — что полностью отменяет действия пользователя.
    auto saveRowChange = [&](const RowChange &c, const char *direction) {
      const std::optional<std::string> colName  = c.toDelete ? std::nullopt : std::optional<std::string>(c.colName);
      const std::optional<std::string> colValue = c.toDelete ? std::nullopt : c.value;
      txn_.exec("INSERT INTO " + sq +
                    ".undo_row_change (id, undo_op_id, table_name, id_value, to_delete, direction, col_name, col_value) "
                    "VALUES ($1, $2, $3, $4, $5, $6, $7, $8)",
                pqxx::params{txn_, new_id(), opId, c.tableName, c.idValue, c.toDelete, direction, colName, colValue});
    };

    for (const RowChange &c : userChanges) {
      saveRowChange(c, "Forward");
    }
    for (const RowChange &c : undoChanges) {
      saveRowChange(c, "ForUndo");
    }

    // 6. Применить пользовательские изменения к реальным таблицам данных.
    for (const RowChange &c : userChanges) {
      applyRowChange(txn_, pg_, schema_, c);
    }

    // 7. Отметить, что в буфере произошло изменение.
    txn_.exec("UPDATE " + sq + ".undo_buffer SET updated_at = now() WHERE id = $1", pqxx::params{txn_, bufferId});
  }
  bool ChangeSystem::undo(const std::string &targetId, bool grouped) const
  {}
  bool ChangeSystem::redo(const std::string &targetId, bool grouped) const
  {}
} // namespace back