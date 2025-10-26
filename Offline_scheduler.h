#pragma once
#include <string>
#include <cstdint>
#include <sys/time.h>
#include <vector>
#include <sstream>

using namespace std;
struct Process {
    string command;
    bool finished = false;
    bool error = false;
    uint64_t start_time = 0;
    uint64_t completion_time = 0;
    uint64_t turnaround_time = 0;
    uint64_t waiting_time = 0;
    uint64_t response_time = 0;
    bool started = false;
    int process_id = -1;
};

inline uint64_t get_current_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return static_cast<uint64_t>(tv.tv_sec) * 1000 + tv.tv_usec / 1000;
}

inline void parse_command(const std::string& command, std::vector<char*>& argv, std::vector<std::string>& tokens) {
    istringstream iss(command);
    string tok;
    while (iss >> tok) tokens.push_back(tok);
    argv.clear();
    for (auto& s : tokens) argv.push_back(const_cast<char*>(s.c_str()));
    argv.push_back(nullptr);
}
