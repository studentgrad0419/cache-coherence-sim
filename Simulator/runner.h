// runner.h describes which coherency is implemented and runs it
// custom delay comparator so we can simulate a random memory fetch delay if we wanted to.
#ifndef RUNNER_H
#define RUNNER_H

#include "Cache.h"
#include "cache_controller.h"
#include "MESI_controller.h"
#include "Bus.h"
#include "metrics.h"
#include <vector>
#include <string>
#include <queue>

// Enum representing different Coherency types
enum class CacheCoherency {
    MESI,
    MOESI,
    MESIF,
    Dragon,
};

// Custom comparator for the priority queue
struct DelayedResponseComparator {
    bool operator()(const std::tuple<int, BusMessage, ResponseMessageType>& lhs,
                    const std::tuple<int, BusMessage, ResponseMessageType>& rhs) const {
        // Implement your comparison logic here
        // You might want to compare based on the first element of the tuple, for example
        return std::get<0>(lhs) > std::get<0>(rhs);
    }
};

void runSim(CacheCoherency cc_type, char* filename, Metrics* metric, int associativity, int block_size, int num_cache, int num_blocks, int mem_delay);

#endif // RUNNER_H