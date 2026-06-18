#include "DbTestBase.h"
#include "back/InitDb.h"
#include <gtest/gtest.h>

using namespace back;

class InitDbTest : public DbTest
{
protected:
  void init_repo()
  {
    pqxx::work txn(*pg);
    InitDb(txn, *pg, schema).init_repo_schema();
    txn.commit();
  }
};

// ---------------------------------------------------------------------------
// init_repo_schema
// ---------------------------------------------------------------------------

TEST_F(InitDbTest, InitRepoSchemaCreatesSchema)
{
  //
  //
  init_repo();
  //
  //

  EXPECT_TRUE(schema_exists(schema));
}

TEST_F(InitDbTest, InitRepoSchemaCreatesLangSettingTable)
{
  //
  //
  init_repo();
  //
  //

  EXPECT_TRUE(table_exists("lang_setting"));
  EXPECT_TRUE(column_exists("lang_setting", "name"));
  EXPECT_TRUE(column_exists("lang_setting", "value"));
}

TEST_F(InitDbTest, InitRepoSchemaCreatesFolderTable)
{
  //
  //
  init_repo();
  //
  //

  EXPECT_TRUE(table_exists("folder"));
  EXPECT_TRUE(column_exists("folder", "id"));
  EXPECT_TRUE(column_exists("folder", "parent_id"));
  EXPECT_TRUE(column_exists("folder", "name"));
}

TEST_F(InitDbTest, InitRepoSchemaAddsAuditColumnsToLangSetting)
{
  //
  //
  init_repo();
  //
  //

  EXPECT_TRUE(column_exists("lang_setting", "created_at"));
  EXPECT_TRUE(column_exists("lang_setting", "last_modified_at"));
}

TEST_F(InitDbTest, InitRepoSchemaIsIdempotent)
{
  init_repo(); // first run

  pqxx::work txn(*pg);
  //
  //
  EXPECT_NO_THROW(InitDb(txn, *pg, schema).init_repo_schema());
  //
  //
  txn.commit();

  EXPECT_TRUE(table_exists("lang_setting"));
  EXPECT_TRUE(table_exists("folder"));
}

// ---------------------------------------------------------------------------
// init_folder_table
// ---------------------------------------------------------------------------

TEST_F(InitDbTest, InitFolderTableCreatesTable)
{
  make_schema();
  ASSERT_FALSE(table_exists("folder"));

  {
    pqxx::work txn(*pg);
    //
    //
    InitDb(txn, *pg, schema).init_folder_table();
    //
    //
    txn.commit();
  }

  EXPECT_TRUE(table_exists("folder"));
  EXPECT_TRUE(column_exists("folder", "id"));
  EXPECT_TRUE(column_exists("folder", "parent_id"));
  EXPECT_TRUE(column_exists("folder", "name"));
}

TEST_F(InitDbTest, InitFolderTableIsIdempotent)
{
  make_schema();
  {
    pqxx::work txn(*pg);
    InitDb(txn, *pg, schema).init_folder_table(); // first creation
    txn.commit();
  }

  pqxx::work txn(*pg);
  //
  //
  EXPECT_NO_THROW(InitDb(txn, *pg, schema).init_folder_table());
  //
  //
  txn.commit();
}
