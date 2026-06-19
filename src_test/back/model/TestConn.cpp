#include "back/model/Conn.h"
#include <gtest/gtest.h>

#include <string>

// ---------------------------------------------------------------------------
// Conn::dsn — builds a libpq connection string; pure, no DB needed.
// ---------------------------------------------------------------------------

TEST(ConnDsn, IncludesProvidedFields)
{
  back::model::Conn c;
  c.host   = "myhost";
  c.port   = "1234";
  c.dbname = "mydb";
  c.user   = "me";
  c.pass   = "pw";

  //
  //
  const std::string cs = c.dsn();
  //
  //

  EXPECT_NE(cs.find("host=myhost"), std::string::npos);
  EXPECT_NE(cs.find("port=1234"), std::string::npos);
  EXPECT_NE(cs.find("dbname=mydb"), std::string::npos);
  EXPECT_NE(cs.find("user=me"), std::string::npos);
  EXPECT_NE(cs.find("password=pw"), std::string::npos);
}

TEST(ConnDsn, AppliesDefaultsAndOmitsEmptyCredentials)
{
  back::model::Conn c;
  c.host = "h"; // port, dbname, user, pass left empty

  //
  //
  const std::string cs = c.dsn();
  //
  //

  EXPECT_NE(cs.find("port=5432"), std::string::npos);
  EXPECT_NE(cs.find("dbname=postgres"), std::string::npos);
  EXPECT_EQ(cs.find("user="), std::string::npos);     // empty user omitted
  EXPECT_EQ(cs.find("password="), std::string::npos); // empty pass omitted
}
