#include "back/ConnService.h"
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

class ConnServiceTest : public ::testing::Test
{
protected:
  fs::path                   tmp_home;
  std::optional<std::string> old_home;

  void SetUp() override
  {
    if (const char *h = std::getenv("HOME")) old_home = h;

    // Unique temp dir: <system-temp>/connsvc_XXXXXX
    std::string tmpl = (fs::temp_directory_path() / "connsvc_XXXXXX").string();
    std::vector<char> buf(tmpl.begin(), tmpl.end());
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
  fs::path conn_file(const std::string &name) const
  {
    return tmp_home / ".config" / "probe_lang" / "workspace" / (name + ".pg-connect");
  }

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

TEST_F(ConnServiceTest, WsDirIsUnderHome)
{
  EXPECT_EQ(back::ws_dir(), tmp_home / ".config" / "probe_lang" / "workspace");
}

// ---------------------------------------------------------------------------
// load_all — empty state
// ---------------------------------------------------------------------------

TEST_F(ConnServiceTest, LoadAllOnMissingDirReturnsEmpty)
{
  // Workspace dir does not exist yet.
  EXPECT_TRUE(back::load_all().empty());
}

TEST_F(ConnServiceTest, LoadAllOnEmptyDirReturnsEmpty)
{
  fs::create_directories(back::ws_dir());
  EXPECT_TRUE(back::load_all().empty());
}

// ---------------------------------------------------------------------------
// save_conn
// ---------------------------------------------------------------------------

TEST_F(ConnServiceTest, SaveCreatesFile)
{
  back::save_conn(make_conn("prod"));
  EXPECT_TRUE(fs::exists(conn_file("prod")));
}

TEST_F(ConnServiceTest, SaveThenLoadRoundTripsAllFields)
{
  Conn c = make_conn("prod");
  back::save_conn(c);

  std::vector<Conn> all = back::load_all();
  ASSERT_EQ(all.size(), 1u);

  const Conn &got = all[0];
  EXPECT_EQ(got.name, "prod"); // name comes from the file stem
  EXPECT_EQ(got.host, c.host);
  EXPECT_EQ(got.port, c.port);
  EXPECT_EQ(got.dbname, c.dbname);
  EXPECT_EQ(got.user, c.user);
  EXPECT_EQ(got.pass, c.pass);
  EXPECT_TRUE(got.connected);
}

TEST_F(ConnServiceTest, ConnectedFalseRoundTrips)
{
  Conn c      = make_conn("local");
  c.connected = false;
  back::save_conn(c);

  std::vector<Conn> all = back::load_all();
  ASSERT_EQ(all.size(), 1u);
  EXPECT_FALSE(all[0].connected);
}

TEST_F(ConnServiceTest, EmptyOptionalFieldsRoundTrip)
{
  Conn c;
  c.name = "minimal"; // everything else empty, connected=false
  back::save_conn(c);

  std::vector<Conn> all = back::load_all();
  ASSERT_EQ(all.size(), 1u);
  EXPECT_EQ(all[0].name, "minimal");
  EXPECT_TRUE(all[0].host.empty());
  EXPECT_TRUE(all[0].port.empty());
  EXPECT_TRUE(all[0].dbname.empty());
  EXPECT_TRUE(all[0].user.empty());
  EXPECT_TRUE(all[0].pass.empty());
  EXPECT_FALSE(all[0].connected);
}

TEST_F(ConnServiceTest, SaveOverwritesExisting)
{
  Conn c = make_conn("prod");
  back::save_conn(c);

  c.host = "new-host";
  back::save_conn(c);

  std::vector<Conn> all = back::load_all();
  ASSERT_EQ(all.size(), 1u);
  EXPECT_EQ(all[0].host, "new-host");
}

// ---------------------------------------------------------------------------
// save_conn with rename (old_name)
// ---------------------------------------------------------------------------

TEST_F(ConnServiceTest, SaveWithRenameRemovesOldFile)
{
  back::save_conn(make_conn("old"));
  ASSERT_TRUE(fs::exists(conn_file("old")));

  Conn renamed = make_conn("new");
  back::save_conn(renamed, "old");

  EXPECT_FALSE(fs::exists(conn_file("old")));
  EXPECT_TRUE(fs::exists(conn_file("new")));

  std::vector<Conn> all = back::load_all();
  ASSERT_EQ(all.size(), 1u);
  EXPECT_EQ(all[0].name, "new");
}

TEST_F(ConnServiceTest, SaveWithSameOldNameKeepsFile)
{
  Conn c = make_conn("same");
  back::save_conn(c);
  back::save_conn(c, "same"); // old_name == new name: must not delete itself

  EXPECT_TRUE(fs::exists(conn_file("same")));
  EXPECT_EQ(back::load_all().size(), 1u);
}

// ---------------------------------------------------------------------------
// delete_conn
// ---------------------------------------------------------------------------

TEST_F(ConnServiceTest, DeleteRemovesFile)
{
  back::save_conn(make_conn("doomed"));
  ASSERT_TRUE(fs::exists(conn_file("doomed")));

  back::delete_conn("doomed");
  EXPECT_FALSE(fs::exists(conn_file("doomed")));
  EXPECT_TRUE(back::load_all().empty());
}

// ---------------------------------------------------------------------------
// load_all — multiple entries, sorting, filtering
// ---------------------------------------------------------------------------

TEST_F(ConnServiceTest, LoadAllReturnsSortedByName)
{
  back::save_conn(make_conn("charlie"));
  back::save_conn(make_conn("alpha"));
  back::save_conn(make_conn("bravo"));

  std::vector<Conn> all = back::load_all();
  ASSERT_EQ(all.size(), 3u);
  EXPECT_EQ(all[0].name, "alpha");
  EXPECT_EQ(all[1].name, "bravo");
  EXPECT_EQ(all[2].name, "charlie");
}

TEST_F(ConnServiceTest, LoadAllIgnoresForeignExtensions)
{
  back::save_conn(make_conn("real"));

  // Drop an unrelated file into the workspace dir.
  fs::path stray = back::ws_dir() / "notes.txt";
  std::ofstream(stray) << "host=should-be-ignored\n";

  std::vector<Conn> all = back::load_all();
  ASSERT_EQ(all.size(), 1u);
  EXPECT_EQ(all[0].name, "real");
}

TEST_F(ConnServiceTest, LoadAllIgnoresUnknownKeysAndBlankLines)
{
  fs::create_directories(back::ws_dir());
  std::ofstream(back::ws_dir() / "manual.pg-connect")
      << "host=h\n"
      << "\n"                 // blank line
      << "garbage-without-eq\n" // no '='
      << "unknown=value\n"    // unknown key
      << "port=1234\n"
      << "connected=YES\n";

  std::vector<Conn> all = back::load_all();
  ASSERT_EQ(all.size(), 1u);
  EXPECT_EQ(all[0].name, "manual");
  EXPECT_EQ(all[0].host, "h");
  EXPECT_EQ(all[0].port, "1234");
  EXPECT_TRUE(all[0].connected);
}

TEST_F(ConnServiceTest, ConnectedNonYesParsesAsFalse)
{
  fs::create_directories(back::ws_dir());
  std::ofstream(back::ws_dir() / "weird.pg-connect") << "connected=maybe\n";

  std::vector<Conn> all = back::load_all();
  ASSERT_EQ(all.size(), 1u);
  EXPECT_FALSE(all[0].connected); // only the literal "YES" means connected
}
