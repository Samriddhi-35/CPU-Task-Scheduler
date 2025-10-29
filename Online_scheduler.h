#pragma once
#include <string>
#include <cstdint>
struct OnlineProcess {
    std::string command;
    bool finished = false, error = false, started = false;
    pid_t pid = -1;
    uint64_t arrival_time = 0, completion_time = 0, turnaround_time = 0, waiting_time = 0, response_time = 0, total_cpu_time = 0;
};
class OnlineScheduler {
public:
    void ShortestJobFirst(int k);
    void MultiLevelFeedbackQueue(int q0, int q1, int q2, int boostTime);
};
