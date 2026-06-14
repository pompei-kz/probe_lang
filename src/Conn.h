#pragma once
#include <string>
#include <vector>
#include <filesystem>

struct Conn {
    std::string name, host, port, user, pass;
};

std::filesystem::path ws_dir();
std::vector<Conn>     load_all();
void                  save_conn(const Conn& c, const std::string& old_name = "");
