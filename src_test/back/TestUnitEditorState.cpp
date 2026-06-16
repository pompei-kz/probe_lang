#include "back/UnitEditorState.h"
#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace back;

// UnitEditorState stores one file per unit under
// $HOME/.config/probe_lang/unit_editor_sys_coord/. The directory is derived from
// $HOME, so the tests redirect $HOME to a throw-away temp directory.
class UnitEditorStateTest : public testing::Test
{
protected:
  fs::path                   tmp_home;
  std::optional<std::string> old_home;

  void SetUp() override
  {
    if (const char *h = std::getenv("HOME")) old_home = h;

    std::string tmpl = (fs::temp_directory_path() / "ues_XXXXXX").string();
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

  fs::path file_for(const std::string &unit_id) const { return tmp_home / ".config" / "probe_lang" / "unit_editor_sys_coord" / unit_id; }
};

TEST_F(UnitEditorStateTest, DirIsUnderHome)
{
  //
  //
  const fs::path dir = unit_editor_sys_coord_dir();
  //
  //

  EXPECT_EQ(dir, tmp_home / ".config" / "probe_lang" / "unit_editor_sys_coord");
}

TEST_F(UnitEditorStateTest, LoadMissingReturnsNullopt)
{
  //
  //
  const auto st = load_unit_editor_coord("nope");
  //
  //

  EXPECT_FALSE(st.has_value());
}

TEST_F(UnitEditorStateTest, SaveCreatesFileNamedByUnitId)
{
  //
  //
  save_unit_editor_coord("unit42", {1.5, 10.0, 20.0});
  //
  //

  EXPECT_TRUE(fs::is_regular_file(file_for("unit42")));
}

TEST_F(UnitEditorStateTest, SaveThenLoadRoundTrips)
{
  save_unit_editor_coord("u1", {2.25, -123.5, 678.75});

  //
  //
  const auto st = load_unit_editor_coord("u1");
  //
  //

  ASSERT_TRUE(st.has_value());
  EXPECT_DOUBLE_EQ(st->zoom, 2.25);
  EXPECT_DOUBLE_EQ(st->cam_x, -123.5);
  EXPECT_DOUBLE_EQ(st->cam_y, 678.75);
}

TEST_F(UnitEditorStateTest, SaveOverwritesPreviousState)
{
  save_unit_editor_coord("u1", {1.0, 0.0, 0.0});

  //
  //
  save_unit_editor_coord("u1", {3.0, 5.0, 6.0});
  //
  //

  const auto st = load_unit_editor_coord("u1");
  ASSERT_TRUE(st.has_value());
  EXPECT_DOUBLE_EQ(st->zoom, 3.0);
  EXPECT_DOUBLE_EQ(st->cam_x, 5.0);
  EXPECT_DOUBLE_EQ(st->cam_y, 6.0);
}

TEST_F(UnitEditorStateTest, FileHoldsKeyValueLines)
{
  save_unit_editor_coord("u1", {1.5, 10.0, 20.0});

  //
  //
  std::ifstream f(file_for("u1"));
  //
  //

  std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  EXPECT_NE(content.find("zoom=1.5"), std::string::npos);
  EXPECT_NE(content.find("cam_x=10"), std::string::npos);
  EXPECT_NE(content.find("cam_y=20"), std::string::npos);
}

TEST_F(UnitEditorStateTest, MissingKeysKeepDefaults)
{
  fs::create_directories(file_for("partial").parent_path());
  {
    std::ofstream f(file_for("partial"));
    f << "zoom=4\n"; // no cam_x / cam_y
  }

  //
  //
  const auto st = load_unit_editor_coord("partial");
  //
  //

  ASSERT_TRUE(st.has_value());
  EXPECT_DOUBLE_EQ(st->zoom, 4.0);
  EXPECT_DOUBLE_EQ(st->cam_x, 0.0);
  EXPECT_DOUBLE_EQ(st->cam_y, 0.0);
}
