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
using back::model::ConnStore;

// ---------------------------------------------------------------------------
// Fixture
//
// ConnService persists connections under $HOME/.config/probe_lang/workspace.
// ws_dir() reads $HOME via std::getenv on every call, so the tests redirect
// $HOME to a throw-away temp directory. The real ~/.config is never touched.
// ---------------------------------------------------------------------------

class ConnServiceRWTest : public testing::Test
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

  static ConnStore make_conn(const std::string &name)
  {
    ConnStore c;
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
// save_conn
// ---------------------------------------------------------------------------

TEST_F(ConnServiceRWTest, SaveCreatesFile)
{
  //
  //
  back::save_conn(make_conn("prod"));
  //
  //

  EXPECT_TRUE(fs::exists(conn_file("prod")));
}

TEST_F(ConnServiceRWTest, SaveThenLoadRoundTripsAllFields)
{
  ConnStore c = make_conn("prod");

  //
  //
  back::save_conn(c);
  //
  //

  std::vector<ConnStore> all = back::load_all();
  ASSERT_EQ(all.size(), 1u);

  // ReSharper disable once CppUseStructuredBinding
  const ConnStore &got = all[0];

  EXPECT_EQ(got.name, "prod"); // name comes from the file stem
  EXPECT_EQ(got.host, c.host);
  EXPECT_EQ(got.port, c.port);
  EXPECT_EQ(got.dbname, c.dbname);
  EXPECT_EQ(got.user, c.user);
  EXPECT_EQ(got.pass, c.pass);
  EXPECT_TRUE(got.connected);
}

TEST_F(ConnServiceRWTest, ConnectedFalseRoundTrips)
{
  ConnStore c      = make_conn("local");
  c.connected = false;

  //
  //
  back::save_conn(c);
  //
  //

  const std::vector<ConnStore> all = back::load_all();
  ASSERT_EQ(all.size(), 1u);
  EXPECT_FALSE(all[0].connected);
}

TEST_F(ConnServiceRWTest, EmptyOptionalFieldsRoundTrip)
{
  ConnStore c;
  c.name = "minimal"; // everything else empty, connected=false

  //
  //
  back::save_conn(c);
  //
  //

  std::vector<ConnStore> all = back::load_all();
  ASSERT_EQ(all.size(), 1u);
  EXPECT_EQ(all[0].name, "minimal");
  EXPECT_TRUE(all[0].host.empty());
  EXPECT_TRUE(all[0].port.empty());
  EXPECT_TRUE(all[0].dbname.empty());
  EXPECT_TRUE(all[0].user.empty());
  EXPECT_TRUE(all[0].pass.empty());
  EXPECT_FALSE(all[0].connected);
}

TEST_F(ConnServiceRWTest, SaveOverwritesExisting)
{
  ConnStore c = make_conn("prod");
  back::save_conn(c);
  c.host = "new-host";

  //
  //
  back::save_conn(c);
  //
  //

  const std::vector<ConnStore> all = back::load_all();
  ASSERT_EQ(all.size(), 1u);
  EXPECT_EQ(all[0].host, "new-host");
}

// ---------------------------------------------------------------------------
// save_conn with rename (old_name)
// ---------------------------------------------------------------------------

TEST_F(ConnServiceRWTest, SaveWithRenameRemovesOldFile)
{
  back::save_conn(make_conn("old"));
  ASSERT_TRUE(fs::exists(conn_file("old")));

  //
  //
  back::save_conn(make_conn("new"), "old");
  //
  //

  EXPECT_FALSE(fs::exists(conn_file("old")));
  EXPECT_TRUE(fs::exists(conn_file("new")));

  const std::vector<ConnStore> all = back::load_all();
  ASSERT_EQ(all.size(), 1u);
  EXPECT_EQ(all[0].name, "new");
}

TEST_F(ConnServiceRWTest, SaveWithSameOldNameKeepsFile)
{
  const ConnStore c = make_conn("same");
  back::save_conn(c);

  //
  //
  back::save_conn(c, "same"); // old_name == new name: must not delete itself
  //
  //

  EXPECT_TRUE(fs::exists(conn_file("same")));
  EXPECT_EQ(back::load_all().size(), 1u);
}

// ---------------------------------------------------------------------------
// delete_conn
// ---------------------------------------------------------------------------

TEST_F(ConnServiceRWTest, DeleteRemovesFile)
{
  back::save_conn(make_conn("doomed"));
  ASSERT_TRUE(fs::exists(conn_file("doomed")));

  //
  //
  back::delete_conn("doomed");
  //
  //

  EXPECT_FALSE(fs::exists(conn_file("doomed")));
  EXPECT_TRUE(back::load_all().empty());
}
