#include "Offline_scheduler.h"
#include "Online_scheduler.h"
#include <vector>
#include <string>
#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <unistd.h>
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
    cout << "All Offline scheduling algorithms executed.\n";
   
    cout << "=== Online Shortest Job First (SJF) Test ===\n";
    cout << "Enter shell commands (one per line). Example:\n";
    cout << "  sleep 1\n  echo Hello\n  ls -l\n";
    cout << "(Press Ctrl+D or close stdin to stop)\n\n";

     OnlineScheduler scheduler;
    // Run the SJF algorithm with k = 3 (average of last 3 bursts)
    scheduler.ShortestJobFirst(3);
    
    return 0;
}
