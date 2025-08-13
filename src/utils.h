#pragma once
#include <chrono>
#include <string>
using namespace std;

inline std::string current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
        
    return std::to_string(ms);
}
