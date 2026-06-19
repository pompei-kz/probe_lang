#include "back/DbTestBase.h"
#include "back/service/FolderServiceR.h"
#include "back/service/FolderServiceRW.h"
#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace back;

class FolderServiceRTest : public DbTest
{
protected:
  // id of the first folder with the given name ("" if none).
  std::string folder_id(const std::string &name)
  {
    pqxx::work txn(*pg);
    auto       r = txn.exec("SELECT id FROM " + qual("folder") + " WHERE name=$1", pqxx::params{txn, name});
    return r.empty() ? "" : r[0][0].as<std::string>();
  }

  long folder_count()
  {
    pqxx::work txn(*pg);
    return txn.exec("SELECT count(*) FROM " + qual("folder"))[0][0].as<long>();
  }
};

// ---------------------------------------------------------------------------
// load_repo_folders / load_folders_for_schema
// ---------------------------------------------------------------------------

TEST_F(FolderServiceRTest, LoadRepoFoldersWithoutFolderTableReturnsEmpty)
{
  make_schema(); // schema exists, but no folder table

  std::vector<model::FolderNode> roots;
  //
  //
  auto [ok, msg] = load_repo_folders(conn(), schema, roots);
  //
  //

  EXPECT_TRUE(ok) << msg;
  EXPECT_TRUE(roots.empty());
}

TEST_F(FolderServiceRTest, LoadFoldersForSchemaBuildsTree)
{
  make_schema();
  ASSERT_TRUE(create_folder(conn(), schema, "", "Root").first);
  const std::string root = folder_id("Root");
  ASSERT_TRUE(create_folder(conn(), schema, root, "Child").first);

  pqxx::work txn(*pg);
  //
  //
  auto tree = load_folders_for_schema(txn, *pg, schema);
  //
  //
  txn.commit();

  ASSERT_EQ(tree.size(), 1u);
  EXPECT_EQ(tree[0].name, "Root");
  ASSERT_EQ(tree[0].children.size(), 1u);
  EXPECT_EQ(tree[0].children[0].name, "Child");
}
