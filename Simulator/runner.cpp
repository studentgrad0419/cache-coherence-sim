// Defines and runs the main processing loop for snoopy based protocols
// Implementation is trace and tick based
// This type of simulation reflects interleaving of coherency in a multi thread scenario

#include "Cache.h"
#include "cache_controller.h"
#include "MESI_controller.h"
#include "Bus.h"
#include "metrics.h"
#include "runner.h"
#include <memory>
#include <vector>
#include <string>
void initialize_metrics(Metrics* metric, int associativity, int block_size, int num_cache, int num_blocks);

void runSim(CacheCoherency cc_type, char* filename, Metrics* metric, int associativity, int block_size, int num_cache, int num_blocks, int mem_delay){
    
    //initialize metrics
    initialize_metrics(metric, associativity, block_size, num_cache, num_blocks);
    
    //initialize caches
    std::vector<Cache> caches;
    for (int i = 0; i < num_cache; ++i) {
        //size of cache = total number of blocks
        caches.emplace_back(num_blocks, associativity);
    }

    //initialize bus
    Bus bus;

    //initialize controlers
    // std::vector<CacheController> cc_list;
    std::vector<std::unique_ptr<CacheController>> cc_list;
    for (int i = 0; i < num_cache; ++i) {
        switch (cc_type) {
            case CacheCoherency::MESI:
                // cc_list.emplace_back(MESIController(caches[i], metric, i, bus));
                cc_list.push_back(std::unique_ptr<MESIController>(new MESIController(caches[i], metric, i, bus)));
                break;
            // Add other cache coherency types
            default:
                // Handle invalid cache coherency type
                std::cerr << "Error: Invalid cache coherency type" << std::endl;
                return;
        }
    }

    // Open the memory trace file
    std::ifstream inputFile(filename);
    if (!inputFile.is_open()) {
        std::cerr << "Error: Unable to open file " << filename << std::endl;
        return;
    }

    //define a delay queue for memory access
    std::priority_queue<std::tuple<int, BusMessage, ResponseMessageType>,
                    std::vector<std::tuple<int, BusMessage, ResponseMessageType>>,
                    DelayedResponseComparator> delayedResponses;

    int currentTime = 0;
    int currentLineTime;
    std::string line;
    std::getline(inputFile, line);
    bool hasLines = true;
    while(hasLines || !bus.messageQueue.empty() || !delayedResponses.empty()) {
        sscanf(line.c_str(), "%d", &currentLineTime);
        std::cout << "Time: "<< currentTime << " Line Time: " << currentLineTime << "\n";

        // Process lines with the same time
        while (currentLineTime == currentTime) {
            // Process the line
            int time, thread, size;
            char address[9] = {0};
            int readWriteBit;
            sscanf(line.c_str(), "%d:%d:%8[^:]:%d:%d", &time, &thread, address, &readWriteBit, &size);
            int memoryAddress = std::stoi(address, 0, 16);
            int startAddress = memoryAddress - (memoryAddress % block_size);
            int fullSize = size + memoryAddress - startAddress;

            // enqueue relevant cache coherency controller
            cc_list[thread]->enqueueRequest({time, thread, startAddress, readWriteBit});
            //Case the bits are on 2 sides of cache boundary
            if(fullSize > block_size) cc_list[thread]->enqueueRequest({time, thread, startAddress+block_size, readWriteBit});
            std::cout << line << "\n";

            // Read the next line
            if (!std::getline(inputFile, line)) {
                hasLines = false;
                break;  // End of file
            }

            // Extract time from the next line
            sscanf(line.c_str(), "%d", &currentLineTime);
        }

        //Simulate cache controller processing
        for (auto& cc : cc_list) {
            cc->processRequest();//see relevant controller methods/logic
        }

        // Process bus messages / and note any data repsonses
        ResponseMessageType hasDataSent = ResponseMessageType::NO_ACK;
        BusMessage message = {-1, -1, BusMessageType::NO_MSG};
        ResponseMessageType response;
        if(!bus.messageQueue.empty()){
            message = bus.messageQueue.front();
            bus.messageQueue.pop();
            // Broadcast the message to all controllers except for the sender
            for (auto& cc : cc_list) {
                //simulate responding to source but tracking using this logic
                response = cc->processBusMessage(message); //This response to a broadcast
                if(response == ResponseMessageType::ACK_CACHE_TO_CACHE){
                    //Data is sent from cache to cache, have the cache respond
                    hasDataSent = response;
                }
            }
        }

        //Process if response uses data from cache or from memory 
        if(hasDataSent == ResponseMessageType::ACK_CACHE_TO_CACHE) {
          response = hasDataSent;
          cc_list[message.originThread]->processBusResponse(message, response);
        }
        //Bus forwards to memory if it's get request (adds a delay)
        else if(message.address != -1 && (
            message.type == BusMessageType::GetS || 
            message.type == BusMessageType::GetM 
        )){
            response = ResponseMessageType::ACK_DATA_FROM_MEM;
            delayedResponses.push({currentTime + mem_delay, message, response});
        }

        // Check for delayed responses from memory 
        //(mutually exclusive with other responses due to the block on new requests when waiting)
        while(!delayedResponses.empty() && std::get<0>(delayedResponses.top()) <= currentTime) {
            // Process the delayed response
            BusMessage oldMessage = std::get<1>(delayedResponses.top());
            cc_list[oldMessage.originThread]->processBusResponse(oldMessage, std::get<2>(delayedResponses.top()));
            delayedResponses.pop();
        }
        // Increment the current time
        ++currentTime;
    }

}

void initialize_metrics(Metrics* metric, int associativity, int block_size, int num_cache, int num_blocks){
    //Initialize metrics
    metric->total_msg = 0;
    metric->total_ack = 0;
    metric->total_inval = 0;
    metric->total_write_back = 0;
    metric->total_read_mem = 0;
    metric->total_cache_to_cache = 0;

    for (int i = 0; i < num_cache; ++i) {
        // Initialize individual cache metrics
        metric->cacheMetrics[i].bytes_per_set = associativity * block_size;
        metric->cacheMetrics[i].bytes_per_block = block_size;
        metric->cacheMetrics[i].bytes_total = num_blocks * block_size;
        metric->cacheMetrics[i].cache_miss = 0;
        metric->cacheMetrics[i].cache_hit = 0;
    }
}