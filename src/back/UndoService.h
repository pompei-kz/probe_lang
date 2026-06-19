//
// Created by pompei on 2026-06-18.
//

#pragma once
#include "model/UndoRowChange.h"

#include <pqxx/pqxx>
#include <string>
#include <vector>

namespace back {

  class UndoService
  {
    pqxx::work        &txn_;
    pqxx::connection  &pg_;
    const std::string &schema_;

  public:
    UndoService(pqxx::work &txn, pqxx::connection &pg, const std::string &schema);

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
    std::vector<model::UndoRowChange> collectUndoChanges(std::vector<model::UndoRowChange> userChanges) const;
  };

} // namespace back
