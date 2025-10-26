#include "Offline_scheduler.h"
#include <vector>
#include <string>
#include <iostream>
using namespace std;

int main() {
    std::vector<Process> processes = {
        {"ls"},
        {"echo Hello World"},
        {"sleep 1"}
    };

    FCFS(processes);
    write_results_to_csv(processes, "result_offline_FCFS.csv");
    cout << "Results written to result_offline_FCFS.csv"<< endl;
    return 0;
}
