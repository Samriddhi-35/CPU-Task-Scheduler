#pragma once
#include <string>
#include <cstdint>
#include <vector>

using namespace std;
struct OnlineProcess {
    string command;
    bool finished = false, error = false, started = false;
    pid_t pid = -1;
    uint64_t arrival_time = 0, completion_time = 0, turnaround_time = 0, waiting_time = 0, response_time = 0, total_cpu_time = 0;
};
class OnlineScheduler {
public:
    void ShortestJobFirst(int k);
    void MultiLevelFeedbackQueue(int q0, int q1, int q2, int boostTime);
};
struct CmdHistory {
    string cmd;
    vector<double> bursts;
    int next_idx = 0;
    CmdHistory() : bursts(50, 0.0) {}
    void record_burst_to_history(double burst) { bursts[next_idx++] = burst; if (next_idx==50) next_idx=0; }
    double get_avg_burst_ms(int k) const {
        int samples = next_idx > 0 || bursts[0] != 0.0 ? 50 : 0;
        if (samples==0) return -1.0;
        k = min(k,samples);
        double sum = 0.0; int idx = next_idx-1;
        for (int i=0; i<k; ++i) { if (idx<0) idx+=50; sum+=bursts[idx]; --idx; }
        return sum/k;
    }
};
