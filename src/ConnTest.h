#pragma once
#include <string>
#include <utility>

// Returns {true, "Connected"} or {false, error_message}
std::pair<bool, std::string> test_connection(
    const std::string &host,
    const std::string &port,
    const std::string &user,
    const std::string &pass
);
