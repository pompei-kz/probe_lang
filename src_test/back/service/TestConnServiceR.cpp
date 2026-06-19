#include "back/service/ConnServiceR.h"
#include "back/service/ConnServiceRW.h"
#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using back::model::Conn;

// ---------------------------------------------------------------------------
// Fixture
//
// ConnService persists connections under $HOME/.config/probe_lang/workspace.
// ws_dir() reads $HOME via std::getenv on every call, so the tests redirect
// $HOME to a throw-away temp directory. The real ~/.config is never touched.
// ---------------------------------------------------------------------------

class ConnServiceRTest : public testing::Test
{
protected:
  fs::path                   tmp_home;
  std::optional<std::string> old_home;

  void SetUp() override
  {
    if (const char *h = std::getenv("HOME")) old_home = h;

    // Unique temp dir: <system-temp>/connsvc_XXXXXX
    std::string tmpl = (fs::temp_directory_path() / "connsvc_XXXXXX").string();
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

  // Path of the workspace file for a given connection name.
  fs::path conn_file(const std::string &name) const { return tmp_home / ".config" / "probe_lang" / "workspace" / (name + ".pg-connect"); }

  static Conn make_conn(const std::string &name)
  {
    Conn c;
    c.name      = name;
    c.host      = "db.example.com";
    c.port      = "5433";
    c.dbname    = "mydb";
    c.user      = "alice";
    c.pass      = "s3cret";
    c.connected = true;
    return c;
  }
};

// ---------------------------------------------------------------------------
// ws_dir
// ---------------------------------------------------------------------------

TEST_F(ConnServiceRTest, WsDirIsUnderHome)
{
  //
  //
  const fs::path dir = back::ws_dir();
  //
  //

  EXPECT_EQ(dir, tmp_home / ".config" / "probe_lang" / "workspace");
}

// ---------------------------------------------------------------------------
// load_all — empty state
// ---------------------------------------------------------------------------

TEST_F(ConnServiceRTest, LoadAllOnMissingDirReturnsEmpty)
{
  // Workspace dir does not exist yet.
  //
  //
  const std::vector<Conn> all = back::load_all();
  //
  //

  EXPECT_TRUE(all.empty());
}

TEST_F(ConnServiceRTest, LoadAllOnEmptyDirReturnsEmpty)
{
  fs::create_directories(back::ws_dir());

  //
  //
  const std::vector<Conn> all = back::load_all();
  //
  //

  EXPECT_TRUE(all.empty());
}

// ---------------------------------------------------------------------------
// load_all — multiple entries, sorting, filtering
// ---------------------------------------------------------------------------

TEST_F(ConnServiceRTest, LoadAllReturnsSortedByName)
{
  back::save_conn(make_conn("charlie"));
  back::save_conn(make_conn("alpha"));
  back::save_conn(make_conn("bravo"));

  //
  //
  const std::vector<Conn> all = back::load_all();
  //
  //

  ASSERT_EQ(all.size(), 3u);
  EXPECT_EQ(all[0].name, "alpha");
  EXPECT_EQ(all[1].name, "bravo");
  EXPECT_EQ(all[2].name, "charlie");
}

TEST_F(ConnServiceRTest, LoadAllIgnoresForeignExtensions)
{
  back::save_conn(make_conn("real"));

  // Drop an unrelated file into the workspace dir.
  const fs::path stray = back::ws_dir() / "notes.txt";
  std::ofstream(stray) << "host=should-be-ignored\n";

  //
  //
  const std::vector<Conn> all = back::load_all();
  //
  //

  ASSERT_EQ(all.size(), 1u);
  EXPECT_EQ(all[0].name, "real");
}

TEST_F(ConnServiceRTest, LoadAllIgnoresUnknownKeysAndBlankLines)
{
  fs::create_directories(back::ws_dir());
  std::ofstream(back::ws_dir() / "manual.pg-connect") << "host=h\n"
                                                      << "\n"                   // blank line
                                                      << "garbage-without-eq\n" // no '='
                                                      << "unknown=value\n"      // unknown key
                                                      << "port=1234\n"
                                                      << "connected=YES\n";

  //
  //
  const std::vector<Conn> all = back::load_all();
  //
  //

  ASSERT_EQ(all.size(), 1u);
  EXPECT_EQ(all[0].name, "manual");
  EXPECT_EQ(all[0].host, "h");
  EXPECT_EQ(all[0].port, "1234");
  EXPECT_TRUE(all[0].connected);
}

TEST_F(ConnServiceRTest, ConnectedNonYesParsesAsFalse)
{
  fs::create_directories(back::ws_dir());
  std::ofstream(back::ws_dir() / "weird.pg-connect") << "connected=maybe\n";

  //
  //
  const std::vector<Conn> all = back::load_all();
  //
  //

  ASSERT_EQ(all.size(), 1u);
  EXPECT_FALSE(all[0].connected); // only the literal "YES" means connected
}
