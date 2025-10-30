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
#include <csignal>

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

inline int find_history_index(const std::vector<CmdHistory>& ch, const std::string& cmd) {
    for (int i = 0; i < (int)ch.size(); ++i) if (ch[i].cmd == cmd) return i;
    return -1;
}
inline int ensure_history_index(std::vector<CmdHistory>& ch, const std::string& cmd) {
    int idx = find_history_index(ch, cmd);
    if (idx >= 0) return idx;
    CmdHistory new_history;
    new_history.cmd = cmd;
    ch.push_back(new_history);
    return ch.size() - 1;
}

inline void poll_and_enqueue_new_commands(
    std::vector<OnlineProcess>& proc_table,
    std::vector<CmdHistory>& cmd_history,
    uint64_t now)
{
    static std::string leftover;
    char buf[4096];
    ssize_t r = read(STDIN_FILENO, buf, sizeof(buf) - 1);
    if (r > 0) {
        buf[r] = '\0'; leftover += buf;
        size_t p;
        while ((p = leftover.find('\n')) != std::string::npos) {
            std::string cmd = leftover.substr(0, p);
            leftover.erase(0, p + 1);
            int hist_idx = ensure_history_index(cmd_history, cmd);
            OnlineProcess proc{cmd};
            proc.arrival_time = now;
            proc_table.push_back(proc);
        }
    }
}
inline void spawn_and_stop_child(OnlineProcess& p) {
    pid_t pid = fork();
    if (pid == 0) {
        setpgid(0, 0);
        raise(SIGSTOP);
        execl("/bin/sh", "sh", "-c", p.command.c_str(), (char*)NULL);
        std::exit(127);
    } else {
        p.pid = pid;
    }
}
inline bool check_child_exited(pid_t pid, int* status_out) {
    int status;
    pid_t r = waitpid(pid, &status, WNOHANG);
    if (r == 0) return false;
    if (r == -1) return false;
    if (status_out) *status_out = status;
    return true;
}
