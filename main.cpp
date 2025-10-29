#include "Offline_scheduler.h"
#include <vector>
#include <string>
#include <iostream>
using namespace std;

void reset_process_state(std::vector<Process> &processes) {
    for(auto &p : processes){
        p.finished = false;
        p.error = false;
        p.start_time = 0;
        p.completion_time = 0;
        p.turnaround_time = 0;
        p.waiting_time = 0;
        p.response_time = 0;
        p.started = false;
        p.process_id = -1;
    }
}

int main() {
    std::vector<Process> processes = {
        {"ls"},
        {"echo Hello"}
    };

    reset_process_state(processes);
    FCFS(processes);

    reset_process_state(processes);
    RoundRobin(processes, 500); // Time quantum of 500 ms

    reset_process_state(processes);
    MultiLevelFeedbackQueue(processes, 500, 1000, 2000, 4000);
    cout << "All scheduling algorithms executed.\n";
    return 0;
}
