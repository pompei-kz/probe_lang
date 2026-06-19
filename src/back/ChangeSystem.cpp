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

    // Найти буфер отмены цели по её id. Возвращает id буфера или пустую строку,
    // если буфера ещё нет. target_type не используется: цель идентифицируется
    // одним target_id (на цель приходится один буфер).
    std::string findBufferId(pqxx::work &txn, const std::string &sq, const std::string &targetId)
    {
      pqxx::result r = txn.exec("SELECT id FROM " + sq + ".undo_buffer WHERE target_id = $1", pqxx::params{txn, targetId});
      return r.empty() ? std::string() : std::string(r[0][0].c_str());
    }

    // Применить к таблицам данных все изменения операции `opId` в заданном
    // направлении ('Forward' — повтор пользовательских изменений, 'ForUndo' —
    // их отмена). Итоговое состояние не зависит от порядка изменений внутри
    // одной операции, но порядок фиксируется по `id` ради детерминизма.
    void applyOpChanges(pqxx::work &txn, pqxx::connection &pg, const std::string &schema, const std::string &opId, const char *direction)
    {
      const std::string sq   = pg.quote_name(schema);
      pqxx::result      rows = txn.exec("SELECT table_name, id_value, to_delete, col_name, col_value FROM " + sq +
                                            ".undo_row_change WHERE undo_op_id = $1 AND direction = $2 ORDER BY id",
                                        pqxx::params{txn, opId, std::string(direction)});
      for (const auto &row : rows) {
        RowChange c;
        c.tableName = row[0].c_str();
        c.idValue   = row[1].c_str();
        c.toDelete  = row[2].as<bool>();
        c.colName   = row[3].is_null() ? std::string() : std::string(row[3].c_str());
        c.value     = row[4].is_null() ? std::nullopt : std::optional<std::string>(row[4].c_str());
        applyRowChange(txn, pg, schema, c);
      }
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
  {
    const std::string sq = pg_.quote_name(schema_);

    // 1. Буфер цели. Нет буфера — отменять нечего.
    const std::string bufferId = findBufferId(txn_, sq, targetId);
    if (bufferId.empty()) return false;

    // 2. Активные (не отменённые) операции буфера, новейшие первыми. Первая —
    //    активная операция, которую можно отменить.
    pqxx::result ops =
        txn_.exec("SELECT id, group_name FROM " + sq + ".undo_op WHERE undo_buffer_id = $1 AND undone = FALSE ORDER BY order_index DESC",
                  pqxx::params{txn_, bufferId});
    if (ops.empty()) return false; // нечего отменять

    // 3. Какие операции отменяем: только активную, либо непрерывную группу
    //    новейших операций с тем же group_name.
    auto groupOf = [](const auto &r) -> std::optional<std::string> {
      return r[1].is_null() ? std::nullopt : std::optional<std::string>(r[1].c_str());
    };
    const std::optional<std::string> activeGroup = groupOf(ops[0]);

    std::vector<std::string> opIds;
    for (const auto &op : ops) {
      if (grouped && groupOf(op) != activeGroup) break; // другая группа — стоп
      opIds.push_back(op[0].c_str());
      if (!grouped) break; // без группировки — только активная операция
    }

    // 4. Отменяем от новейшей к старейшей: применяем ForUndo и помечаем undone.
    for (const std::string &opId : opIds) {
      applyOpChanges(txn_, pg_, schema_, opId, "ForUndo");
      txn_.exec("UPDATE " + sq + ".undo_op SET undone = TRUE WHERE id = $1", pqxx::params{txn_, opId});
    }
    txn_.exec("UPDATE " + sq + ".undo_buffer SET updated_at = now() WHERE id = $1", pqxx::params{txn_, bufferId});
    return true;
  }

  bool ChangeSystem::redo(const std::string &targetId, bool grouped) const
  {
    const std::string sq = pg_.quote_name(schema_);

    // 1. Буфер цели. Нет буфера — возвращать нечего.
    const std::string bufferId = findBufferId(txn_, sq, targetId);
    if (bufferId.empty()) return false;

    // 2. Отменённые операции буфера, старейшие первыми. Первая — та, которую
    //    нужно вернуть следующей.
    pqxx::result ops = txn_.exec("SELECT id, group_name FROM " + sq + ".undo_op WHERE undo_buffer_id = $1 AND undone = TRUE ORDER BY order_index ASC",
                                 pqxx::params{txn_, bufferId});
    if (ops.empty()) return false; // нет отменённых операций — возвращать нечего

    // 3. Какие операции возвращаем: только первую, либо непрерывную группу
    //    отменённых операций с тем же group_name.
    auto groupOf = [](const auto &r) -> std::optional<std::string> {
      return r[1].is_null() ? std::nullopt : std::optional<std::string>(r[1].c_str());
    };
    const std::optional<std::string> firstGroup = groupOf(ops[0]);

    std::vector<std::string> opIds;
    for (const auto &op : ops) {
      if (grouped && groupOf(op) != firstGroup) break; // другая группа — стоп
      opIds.push_back(op[0].c_str());
      if (!grouped) break; // без группировки — только одна операция
    }

    // 4. Возвращаем в исходном порядке применения: повторяем Forward и снимаем undone.
    for (const std::string &opId : opIds) {
      applyOpChanges(txn_, pg_, schema_, opId, "Forward");
      txn_.exec("UPDATE " + sq + ".undo_op SET undone = FALSE WHERE id = $1", pqxx::params{txn_, opId});
    }
    txn_.exec("UPDATE " + sq + ".undo_buffer SET updated_at = now() WHERE id = $1", pqxx::params{txn_, bufferId});
    return true;
  }
} // namespace back