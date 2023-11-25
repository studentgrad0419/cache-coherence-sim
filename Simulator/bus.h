// Bus.h defines message types and utilizes a queue to enforce ordering

#ifndef BUS_H
#define BUS_H

#include <iostream>
#include <queue>

// Enum representing different bus message types
// Relevant ones for different coherency usage (depend on controller what is used/ processed)
// Based on table 6.4, page 102, A Primer on Memory Consistency and Cache Coherence by Vijay Nagarajan, Daniel J. Sorin, Mark D. Hill, and David A. Wood
enum class BusMessageType {
    NO_MSG,     // Default Value 
    GetS,       // Requesting a block in S 
    GetM,       // Requesting a block in M (to be modified)
    Upg,        // From Share/Owned into M
    PutS,       // Evict Block in S state
    PutE,       // Evict Block in E state
    PutO,       // Evict Block in O state
    PutM,       // Evict Block in M state
};

// To support types of ack messages
enum class ResponseMessageType{
    NO_ACK,
    ACK,                
    ACK_CACHE_TO_CACHE,
    ACK_DATA_TO_MEM,
    ACK_DATA_FROM_MEM,
    ACK_NO_DATA,
};

// To give information about broadcasted messages
struct BusMessage {
    int address;
    int originThread;
    BusMessageType type;
};

// Bus Class 
class Bus {
public:
    std::queue<BusMessage> messageQueue;

    // Function to add a new bus request
    void addBusRequest(const BusMessage& request) {
        messageQueue.push(request);
    }
};

#endif // BUS_H
