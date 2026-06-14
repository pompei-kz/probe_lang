#pragma once
#include <filesystem>
#include <string>
#include <vector>

struct Conn
{
  std::string name, host, port, user, pass;
};

std::filesystem::path ws_dir();
std::vector<Conn>     load_all();
void                  save_conn(const Conn &c, const std::string &old_name = "");
void                  delete_conn(const std::string &name);
