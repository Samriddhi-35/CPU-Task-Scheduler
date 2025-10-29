#pragma once
#include <vector>
#include <string>
#include <queue>
#include <ctime>
#include <cstdint>
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/select.h>
#include <thread>
#include <chrono>

using namespace std;
struct OnlineProcess {
    string command;
    bool finished = false, error = false, started = false;
    pid_t pid = -1;
    uint64_t arrival_time = 0, completion_time = 0, turnaround_time = 0, waiting_time = 0, response_time = 0, total_cpu_time = 0;
};

inline uint64_t now_ms() {
    static auto program_start = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - program_start).count();
}

class OnlineScheduler {
public:
    void ShortestJobFirst(int k);
    void MultiLevelFeedbackQueue(int quantum0, int quantum1, int quantum2, int boostTime);
    OnlineScheduler();

private:
    vector<OnlineProcess> proc_table;
    vector<CmdHistory> cmd_histories;
    uint64_t program_start_ms;
    uint64_t now_ms() const;
    void poll_stdin(uint64_t now);
    void spawn_and_stop_child(OnlineProcess &p);
    int check_child_exited(pid_t pid, int* status_out);
};
struct CmdHistory {
    string cmd;
    vector<double> bursts;
    int next_idx = 0;
    CmdHistory() : bursts(50, 0.0) {}
    void add_burst(double burst) { bursts[next_idx] = burst; next_idx = (next_idx + 1) % int(bursts.size()); }
    double avg_burst(int k) const {
        int count = next_idx ? min(next_idx, int(bursts.size())) : 0;
        if (count == 0) return -1.0;
        double sum = 0.0;
        for (int i = 0; i < count; ++i) sum += bursts[i];
        return sum / count;
    }
};

inline void set_stdin_nonblocking(bool enable) {
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags == -1) return;
    if (enable) flags |= O_NONBLOCK;
    else flags &= ~O_NONBLOCK;
    fcntl(STDIN_FILENO, F_SETFL, flags);
}