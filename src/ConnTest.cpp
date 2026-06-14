#include "ConnTest.h"
#include <pqxx/pqxx>

std::pair<bool, std::string> test_connection(
    const std::string &host,
    const std::string &port,
    const std::string &user,
    const std::string &pass)
{
  try {
    std::string cs = "host=" + host;
    cs += " port=" + (port.empty() ? "5432" : port);
    cs += " dbname=postgres";
    if (!user.empty()) cs += " user=" + user;
    if (!pass.empty()) cs += " password=" + pass;
    cs += " connect_timeout=5";
    pqxx::connection c(cs);
    return {true, "Connected successfully"};
  } catch (const std::exception &e) {
    return {false, e.what()};
  }
}
