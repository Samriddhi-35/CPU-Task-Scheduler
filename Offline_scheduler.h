#pragma once
#include <string>
#include <cstdint>
#include <sys/time.h>
#include <vector>
#include <sstream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstdlib>
#include <fstream>
#include <iostream>
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

inline void parse_command(const string& command, vector<char*>& argv, vector<string>& tokens) {
    istringstream iss(command);
    string tok;
    while (iss >> tok) tokens.push_back(tok);
    argv.clear();
    for (auto& s : tokens) argv.push_back(const_cast<char*>(s.c_str()));
    argv.push_back(nullptr);
}

inline void write_results_to_csv(const vector<Process>& processes, const string& filename) {
    ofstream fp(filename);
    if (!fp) { cerr << "Could not open file " << filename << "\n"; return; }
    fp << "Command,Finished,Error,CompletionTime,Turnaround,Waiting,Response\n";
    for (const auto& proc : processes) {
        fp << "\"" << proc.command << "\","
           << (proc.finished ? "Yes" : "No") << ","
           << (proc.error ? "Yes" : "No") << ","
           << proc.completion_time << ","
           << proc.turnaround_time << ","
           << proc.waiting_time << ","
           << proc.response_time << "\n";
    }
}

inline void FCFS(vector<Process>& processes) {
    uint64_t scheduler_start = get_current_time_ms();
    for (auto& proc : processes) {
        vector<string> tokens;
        vector<char*> argv;
        parse_command(proc.command, argv, tokens);

        proc.start_time = get_current_time_ms() - scheduler_start;
        pid_t pid = fork();
        if (pid == 0) {
            execvp(argv[0], argv.data());
            exit(1);
        } else if (pid > 0) {
            proc.process_id = pid;
            proc.started = true;
            int status;
            waitpid(pid, &status, 0);
            proc.completion_time = get_current_time_ms() - scheduler_start;
            proc.finished = WIFEXITED(status) && (WEXITSTATUS(status) == 0);
            proc.error = !proc.finished;
            proc.turnaround_time = proc.completion_time - proc.start_time;
            proc.waiting_time = proc.turnaround_time;
            proc.response_time = proc.start_time;
        }
    }
}

