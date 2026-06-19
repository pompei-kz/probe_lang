#include "back/service/ProjectTreeServiceR.h"
#include "back/service/ProjectTreeServiceRW.h"
#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Fixture
//
// ProjectTreeService stores marker files under
// $HOME/.config/probe_lang/project_tree_open_nodes/. project_tree_open_nodes_dir()
// reads $HOME via std::getenv, so the tests redirect $HOME to a throw-away temp
// directory. The real ~/.config is never touched.
// ---------------------------------------------------------------------------

class ProjectTreeServiceRTest : public testing::Test
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

TEST_F(ProjectTreeServiceRTest, DirIsUnderHome)
{
  //
  //
  const fs::path dir = back::project_tree_open_nodes_dir();
  //
  //

  EXPECT_EQ(dir, tmp_home / ".config" / "probe_lang" / "project_tree_open_nodes");
}

// ---------------------------------------------------------------------------
// is_tree_node_open
// ---------------------------------------------------------------------------

TEST_F(ProjectTreeServiceRTest, IsOpenFalseWhenAbsent)
{
  //
  //
  const bool open = back::is_tree_node_open({"conn"});
  //
  //

  EXPECT_FALSE(open);
}

TEST_F(ProjectTreeServiceRTest, IsOpenTrueAfterOpen)
{
  back::open_tree_node({"conn", "public", "abc"});

  //
  //
  const bool open = back::is_tree_node_open({"conn", "public", "abc"});
  //
  //

  EXPECT_TRUE(open);
}

TEST_F(ProjectTreeServiceRTest, IsOpenFalseAfterClose)
{
  back::open_tree_node({"conn"});
  back::close_tree_node({"conn"});

  //
  //
  const bool open = back::is_tree_node_open({"conn"});
  //
  //

  EXPECT_FALSE(open);
}

TEST_F(ProjectTreeServiceRTest, DistinctPathsAreIndependent)
{
  back::open_tree_node({"conn", "public"});

  //
  //
  const bool other = back::is_tree_node_open({"conn", "private"});
  //
  //

  EXPECT_FALSE(other);
  EXPECT_TRUE(back::is_tree_node_open({"conn", "public"}));
}
