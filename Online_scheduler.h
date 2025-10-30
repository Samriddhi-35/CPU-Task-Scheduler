#pragma once
#include <vector>
#include <string>
#include <queue>
#include <ctime>
#include <cstdint>
#include <iostream>
#include <unistd.h>
#include <fstream>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/select.h>
#include <thread>
#include <chrono>
#include <csignal>
#include <cstring>
#include <cerrno>

using namespace std;

constexpr int MAX_HISTORY = 50;
struct CmdHistory {
    string cmd;
    vector<double> bursts = vector<double>(MAX_HISTORY, 0.0);
    int count = 0;
    int next_idx = 0;
};

struct OnlineProcess {
    string command;
    bool finished = false, error = false, started = false;
    pid_t pid = -1;
    uint64_t arrival_time = 0, completion_time = 0, turnaround_time = 0,
             waiting_time = 0, response_time = 0, total_cpu_time = 0;
    int history_index = -1;
    bool csv_written = false;
    uint64_t slice_start_ms = 0;
};

static struct timespec program_start_ts;

static void set_program_start_time(void) {
    clock_gettime(CLOCK_MONOTONIC, &program_start_ts);
}

static uint64_t now_ms() {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    int64_t sec_diff = static_cast<int64_t>(t.tv_sec) - static_cast<int64_t>(program_start_ts.tv_sec);
    int64_t ns_diff = static_cast<int64_t>(t.tv_nsec) - static_cast<int64_t>(program_start_ts.tv_nsec);
    return static_cast<uint64_t>(sec_diff * 1000LL + ns_diff / 1000000LL);
}

static void record_burst_to_history(vector<CmdHistory>& cmd_history, int hist_idx, double burst_ms) {
    if (hist_idx < 0 || hist_idx >= static_cast<int>(cmd_history.size())) return;
    CmdHistory& h = cmd_history[hist_idx];
    h.bursts[h.next_idx] = burst_ms;
    h.next_idx = (h.next_idx + 1) % MAX_HISTORY;
    if (h.count < MAX_HISTORY) h.count++;
}

static double get_avg_burst_ms(const vector<CmdHistory>& cmd_history, int hist_idx, int k) {
    if (hist_idx < 0 || hist_idx >= static_cast<int>(cmd_history.size())) return -1.0;
    const CmdHistory& h = cmd_history[hist_idx];
    if (h.count == 0) return -1.0;
    int to_take = (k <= 0) ? h.count : min(h.count, k);
    double sum = 0.0;
    int idx = (h.next_idx - 1 + MAX_HISTORY) % MAX_HISTORY;
    for (int i = 0; i < to_take; ++i) {
        sum += h.bursts[idx];
        idx = (idx - 1 + MAX_HISTORY) % MAX_HISTORY;
    }
    return sum / static_cast<double>(to_take);
}

inline void set_stdin_nonblocking(bool enable) {
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags == -1) return;
    if (enable) flags |= O_NONBLOCK;
    else flags &= ~O_NONBLOCK;
    fcntl(STDIN_FILENO, F_SETFL, flags);
}

inline int find_history_index(const vector<CmdHistory>& ch, const string& cmd) {
    for (int i = 0; i < static_cast<int>(ch.size()); ++i)
        if (ch[i].cmd == cmd)
            return i;
    return -1;
}

inline int ensure_history_index(vector<CmdHistory>& ch, const string& cmd) {
    int idx = find_history_index(ch, cmd);
    if (idx >= 0) return idx;
    CmdHistory newEntry;
    newEntry.cmd = cmd;
    ch.push_back(newEntry);
    return static_cast<int>(ch.size()) - 1;
}

inline void spawn_and_stop_child(OnlineProcess& p) {
    pid_t pid = fork();
    if (pid == 0) {
        setpgid(0, 0);
        raise(SIGSTOP);
        execl("/bin/sh", "sh", "-c", p.command.c_str(), (char*)NULL);
        _exit(127);
    } else {
        p.pid = pid;
    }
}

inline bool check_child_exited(pid_t pid, int* status_out) {
    int status;
    pid_t r = waitpid(pid, &status, WNOHANG);
    if (r == 0 || r == -1) return false;
    *status_out = status;
    return true;
}

inline void write_results_to_csv(const vector<OnlineProcess>& processes, const string& filename) {
    ofstream fp(filename);
    if (!fp) {
        cerr << "Could not open file " << filename << "\n";
        return;
    }
    fp << "Command,Finished,Error,CompletionTime,Turnaround,Waiting,Response,TotalCPU\n";
    for (const auto& p : processes) {
        fp << "\"" << p.command << "\","
           << (p.finished ? "Yes" : "No") << ","
           << (p.error ? "Yes" : "No") << ","
           << p.completion_time << ","
           << p.turnaround_time << ","
           << p.waiting_time << ","
           << p.response_time << ","
           << p.total_cpu_time << "\n";
    }
}

inline int poll_and_enqueue_new_commands(
    vector<OnlineProcess>& proc_table,
    vector<CmdHistory>& cmd_history,
    uint64_t now)
{
    static char buf[8192];
    static size_t leftover = 0;
    int added = 0;

    while (true) {
        ssize_t r = read(STDIN_FILENO, buf + leftover, sizeof(buf) - leftover - 1);
        if (r < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            else break;
        } else if (r == 0) {
            break;
        } else {
            leftover += static_cast<size_t>(r);
            buf[leftover] = '\0';
            char* line_start = buf;
            char* nl;

            while ((nl = strchr(line_start, '\n')) != nullptr) {
                size_t linelen = static_cast<size_t>(nl - line_start);
                while (linelen > 0 && (line_start[linelen - 1] == '\r' || line_start[linelen - 1] == '\n'))
                    linelen--;

                string cmd(line_start, linelen);
                if (!cmd.empty()) {
                    OnlineProcess p;
                    p.command = cmd;
                    p.arrival_time = now;
                    p.history_index = ensure_history_index(cmd_history, cmd);

                    spawn_and_stop_child(p);
                    if (p.pid <= 0) {
                        p.error = true;
                        p.finished = true;
                        p.completion_time = now_ms();
                    }

                    proc_table.push_back(p);
                    added++;
                }
                line_start = nl + 1;
            }

            size_t remaining = strlen(line_start);
            memmove(buf, line_start, remaining);
            leftover = remaining;
            buf[leftover] = '\0';
        }
    }
    return added;
}

class OnlineScheduler {
public:
    OnlineScheduler() {
        set_program_start_time();
        program_start_ms = now_ms();
    }

    void ShortestJobFirst(int k);
    void MultiLevelFeedbackQueue(int q0, int q1, int q2, int boostTime);

private:
    vector<OnlineProcess> proc_table;
    vector<CmdHistory> cmd_histories;
    uint64_t program_start_ms;
};

void OnlineScheduler::ShortestJobFirst(int k) {
    set_stdin_nonblocking(true);
    poll_and_enqueue_new_commands(proc_table, cmd_histories, now_ms());

    while (true) {
        poll_and_enqueue_new_commands(proc_table, cmd_histories, now_ms());

        int active = 0;
        for (auto& p : proc_table) if (!p.finished) active++;

        if (active == 0) {
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(STDIN_FILENO, &rfds);
            int sel = select(STDIN_FILENO + 1, &rfds, nullptr, nullptr, nullptr);
            if (sel > 0) continue;
            if (sel == -1 && errno == EINTR) continue;
            break;
        }

        int best_idx = -1;
        double best_est = 1e308;

        for (int i = 0; i < (int)proc_table.size(); ++i) {
            auto& p = proc_table[i];
            if (p.finished) continue;
            double avg = get_avg_burst_ms(cmd_histories, p.history_index, k);
            double est = (avg < 0.0) ? 1000.0 : avg;
            if (est < best_est) {
                best_est = est;
                best_idx = i;
            }
        }

        if (best_idx == -1) break;
        OnlineProcess& job = proc_table[best_idx];
        if (job.pid == -1) spawn_and_stop_child(job);

        kill(-job.pid, SIGCONT);
        if (!job.started) {
            job.started = true;
            job.response_time = now_ms() - job.arrival_time;
        }

        uint64_t start = now_ms();
        while (true) {
            int status = 0;
            if (check_child_exited(job.pid, &status)) {
                uint64_t end = now_ms();
                uint64_t ran = end - start;
                job.finished = true;
                job.total_cpu_time += ran;
                job.completion_time = end;
                job.turnaround_time = job.completion_time - job.arrival_time;
                job.waiting_time = job.turnaround_time - job.total_cpu_time;
                record_burst_to_history(cmd_histories, job.history_index, (double)ran);
                break;
            }
            this_thread::sleep_for(chrono::milliseconds(50));
        }

        write_results_to_csv(proc_table, "result_online_SJF.csv");
    }

    set_stdin_nonblocking(false);
}

static void finalize_proc_metrics(OnlineProcess &p) {
    if (!p.finished) return;

    p.turnaround_time = p.completion_time - p.arrival_time;

    if (p.total_cpu_time > p.turnaround_time)
        p.waiting_time = 0;
    else
        p.waiting_time = p.turnaround_time - p.total_cpu_time;

    if (p.slice_start_ms > 0 && p.response_time == 0 && p.started) {
        p.response_time = p.slice_start_ms - p.arrival_time;
    }
}

static void complete_process(
    OnlineProcess &p,
    uint64_t end_ms,
    bool wstatus_valid,
    int wstatus,
    ofstream &csv,
    vector<CmdHistory> &cmd_history)
{
    if (p.finished) return;

    p.finished = true;
    p.completion_time = end_ms;

    if (wstatus_valid) {
        if (WIFEXITED(wstatus))
            p.error = (WEXITSTATUS(wstatus) != 0);
        else
            p.error = true;
    }

    if (!p.error && p.history_index >= 0) {
        record_burst_to_history(cmd_history, p.history_index, static_cast<double>(p.total_cpu_time));
    }

    finalize_proc_metrics(p);

    cout << "Context switch: " << p.command
         << " | Start: " << p.slice_start_ms
         << " | End: " << end_ms << "\n";

    csv << "\"" << p.command << "\","
        << (p.finished ? "Yes" : "No") << ","
        << (p.error ? "Yes" : "No") << ","
        << p.completion_time << ","
        << p.turnaround_time << ","
        << p.waiting_time << ","
        << p.response_time << ","
        << p.total_cpu_time << "\n";
}

static void print_context_switch(const std::string &cmd, uint64_t start_ms, uint64_t end_ms) {
    std::cout << cmd << ", " << start_ms << ", " << end_ms << std::endl;
    std::cout.flush();
}
