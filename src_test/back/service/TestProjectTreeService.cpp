#include "back/service/ProjectTreeService.h"
#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace back;

// ---------------------------------------------------------------------------
// Fixture
//
// ProjectTreeService stores marker files under
// $HOME/.config/probe_lang/project_tree_open_nodes/. project_tree_open_nodes_dir()
// reads $HOME via std::getenv, so the tests redirect $HOME to a throw-away temp
// directory. The real ~/.config is never touched.
// ---------------------------------------------------------------------------

class ProjectTreeServiceTest : public testing::Test
{
protected:
  fs::path                   tmp_home;
  std::optional<std::string> old_home;

  void SetUp() override
  {
    if (const char *h = std::getenv("HOME")) old_home = h;

    std::string tmpl = (fs::temp_directory_path() / "pts_XXXXXX").string();
    std::vector buf(tmpl.begin(), tmpl.end());
    buf.push_back('\0');
    const char *made = mkdtemp(buf.data());
    ASSERT_NE(made, nullptr) << "mkdtemp failed";
    tmp_home = made;

    ASSERT_EQ(setenv("HOME", tmp_home.c_str(), 1), 0);
  }

  void TearDown() override
  {
    if (old_home)
      setenv("HOME", old_home->c_str(), 1);
    else
      unsetenv("HOME");

    std::error_code ec;
    fs::remove_all(tmp_home, ec);
  }

  fs::path marker(const std::string &name) const { return tmp_home / ".config" / "probe_lang" / "project_tree_open_nodes" / name; }
};

// ---------------------------------------------------------------------------
// project_tree_open_nodes_dir
// ---------------------------------------------------------------------------

TEST_F(ProjectTreeServiceTest, DirIsUnderHome)
{
  //
  //
  const fs::path dir = project_tree_open_nodes_dir();
  //
  //

  EXPECT_EQ(dir, tmp_home / ".config" / "probe_lang" / "project_tree_open_nodes");
}

// ---------------------------------------------------------------------------
// open_tree_node
// ---------------------------------------------------------------------------

TEST_F(ProjectTreeServiceTest, OpenCreatesMarkerFile)
{
  //
  //
  open_tree_node({"conn"});
  //
  //

  EXPECT_TRUE(fs::exists(marker("conn")));
}

TEST_F(ProjectTreeServiceTest, OpenJoinsPathWithHash)
{
  //
  //
  open_tree_node({"conn", "public", "abc"});
  //
  //

  EXPECT_TRUE(fs::exists(marker("conn#public#abc")));
}

TEST_F(ProjectTreeServiceTest, OpenCreatesEmptyFile)
{
  //
  //
  open_tree_node({"conn", "public"});
  //
  //

  EXPECT_TRUE(fs::is_regular_file(marker("conn#public")));
  EXPECT_EQ(fs::file_size(marker("conn#public")), 0u);
}

TEST_F(ProjectTreeServiceTest, OpenIsIdempotent)
{
  open_tree_node({"conn"});

  //
  //
  open_tree_node({"conn"});
  //
  //

  EXPECT_TRUE(fs::exists(marker("conn")));
}

// ---------------------------------------------------------------------------
// close_tree_node
// ---------------------------------------------------------------------------

TEST_F(ProjectTreeServiceTest, CloseRemovesMarkerFile)
{
  open_tree_node({"conn", "public"});
  ASSERT_TRUE(fs::exists(marker("conn#public")));

  //
  //
  close_tree_node({"conn", "public"});
  //
  //

  EXPECT_FALSE(fs::exists(marker("conn#public")));
}

TEST_F(ProjectTreeServiceTest, CloseMissingNodeDoesNotThrow)
{
  //
  //
  EXPECT_NO_THROW(close_tree_node({"never", "created"}));
  //
  //
}

TEST_F(ProjectTreeServiceTest, CloseAffectsOnlyTheGivenNode)
{
  open_tree_node({"conn"});
  open_tree_node({"conn", "public"});

  //
  //
  close_tree_node({"conn", "public"});
  //
  //

  EXPECT_TRUE(fs::exists(marker("conn"))); // sibling untouched
  EXPECT_FALSE(fs::exists(marker("conn#public")));
}

// ---------------------------------------------------------------------------
// is_tree_node_open
// ---------------------------------------------------------------------------

TEST_F(ProjectTreeServiceTest, IsOpenFalseWhenAbsent)
{
  //
  //
  const bool open = is_tree_node_open({"conn"});
  //
  //

  EXPECT_FALSE(open);
}

TEST_F(ProjectTreeServiceTest, IsOpenTrueAfterOpen)
{
  open_tree_node({"conn", "public", "abc"});

  //
  //
  const bool open = is_tree_node_open({"conn", "public", "abc"});
  //
  //

  EXPECT_TRUE(open);
}

TEST_F(ProjectTreeServiceTest, IsOpenFalseAfterClose)
{
  open_tree_node({"conn"});
  close_tree_node({"conn"});

  //
  //
  const bool open = is_tree_node_open({"conn"});
  //
  //

  EXPECT_FALSE(open);
}

TEST_F(ProjectTreeServiceTest, DistinctPathsAreIndependent)
{
  open_tree_node({"conn", "public"});

  //
  //
  const bool other = is_tree_node_open({"conn", "private"});
  //
  //

  EXPECT_FALSE(other);
  EXPECT_TRUE(is_tree_node_open({"conn", "public"}));
}
