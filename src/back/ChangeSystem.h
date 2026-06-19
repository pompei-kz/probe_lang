//
// Created by pompei on 2026-06-18.
//

#pragma once
#include "model/ChangeOp.h"
#include "model/ChangeSysTarget.h"
#include "model/RowChange.h"

#include <pqxx/pqxx>
#include <string>
#include <vector>

namespace back {

  class ChangeSystem
  {
    pqxx::work        &txn_;
    pqxx::connection  &pg_;
    const std::string &schema_;

  public:
    ChangeSystem(pqxx::work &txn, pqxx::connection &pg, const std::string &schema);

    /**
     * Собирает изменения, которые отменяют исходящие.
     *
     * Вызывается ДО применения `userChanges`: текущее (старое) состояние строки
     * (по `id == idValue`) читается из БД, и из него строится изменение,
     * возвращающее это состояние. Отменяющие изменения возвращаются в обратном
     * порядке к исходным.
     *
     * Отмена обновления (`toDelete == false`): если строка `id == idValue` уже
     * есть — она возвращается к прежнему значению колонки `colName`; если строки
     * ещё нет (изменение её создаст) — отменой будет удаление этой строки.
     *
     * Отмена удаления (`toDelete == true`): текущая строка читается из БД, и для
     * каждой её колонки (кроме `id`) создаётся изменение `toDelete == false`.
     * Применённые подряд, эти изменения воссоздают строку (первое создаёт её
     * через upsert по `id`, остальные проставляют оставшиеся колонки). Если
     * строки нет — отменять нечего.
     *
     * @param userChanges Список исходящих изменений в БД
     * @return Список изменений, который отменяет эти изменения
     */
    std::vector<model::RowChange> collectUndoChanges(const std::vector<model::RowChange> &userChanges) const;

    /**
     * Сохраняет изменения в системе изменений для указанной цели.
     * Система изменений в БД состоит из таблиц, которые создаются методом back::InitDb::init_change_system_tables().
     * Также получает изменения отменяющие эти, и тоже их сохраняет в системе изменений для будущего возможного отката.
     * После применяет эти изменения в БД.
     *
     * Примечание:
     * Если буфера для данной цели пока ещё не существует, то он создаётся автоматически.
     * Если в буфере были отменённые операции, то они удаляются, тем самым возможность redo затирается.
     *
     * @param userChanges Список изменений в БД
     * @param operation Имя операции для сохранения в системе изменений
     * @param target К чему относятся эти изменения
     */
    void apply(const std::vector<model::RowChange> &userChanges, const model::ChangeOp &operation, const model::ChangeSysTarget &target) const;
  };

} // namespace back
