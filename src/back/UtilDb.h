#pragma once
#include <pqxx/pqxx>
#include <string>

// Reusable column/trigger helpers shared across tables. Idempotent: safe to
// call on an already-prepared table. Internal backend helper.
namespace back {

  // Ensure the table has a "created_at timestamp DEFAULT now()" column.
  void ensureCreatedAt(pqxx::work &txn, const std::string &schemaName, const std::string &tableName);

  // Ensure the table has a "last_modified_at timestamp" column that is set to
  // now() automatically on every row update (via a BEFORE UPDATE trigger).
  void ensureLastModifiedAt(pqxx::work &txn, const std::string &schemaName, const std::string &tableName);

  /**
   * Проверяет наличие указанной схемы
   * @param txn Транзакция
   * @param schemaName Имя указанной схемы
   * @return Признак наличия указанной схемы
   */
  bool hasSchema(pqxx::work &txn, const std::string &schemaName);

  /**
   * Проверяет существование указанной таблице в указанной схеме
   * @param txn Транзакция
   * @param schemaName Имя указанной схемы
   * @param tableName Имя указанной таблицы
   * @return Признак наличия данной таблицы
   */
  bool hasTable(pqxx::work &txn, const std::string &schemaName, const std::string &tableName);

  /**
   * Проверяет существование указанного индекса в указанной схеме
   * @param txn Транзакция
   * @param schemaName Имя указанной схемы
   * @param indexName Имя указанного индекса
   * @return Признак наличия данного индекса
   */
  bool hasIndex(pqxx::work &txn, const std::string &schemaName, const std::string &indexName);

} // namespace back
