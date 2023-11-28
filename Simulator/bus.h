// Bus.h defines message types and utilizes a queue to enforce ordering

#ifndef BUS_H
#define BUS_H

#include <iostream>
#include <queue>
#include <unordered_set>

// Enum representing different bus message types
// Relevant ones for different coherency usage (depend on controller what is used/ processed)
// Based on table 6.4, page 102, A Primer on Memory Consistency and Cache Coherence by Vijay Nagarajan, Daniel J. Sorin, Mark D. Hill, and David A. Wood
enum class BusMessageType {
    NO_MSG,     // Default Value 
    GetS,       // Requesting a block in S //also be referred to as a read probe
    GetM,       // Requesting a block in M //also referred as a write probe
    Upg,        // From Share/Owned into M //asks others to invalidate, but getM suffices
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

//helperFunctions for enums
std::string busMessageTypeToString(BusMessageType messageType);
std::string responseMessageTypeToString(ResponseMessageType responseType);

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
    //Support for atomic requests -> no state change
    std::unordered_set<int> activeTransactions;  // Track active transactions per block
    
    // Function to add a new bus request
    void addBusRequest(const BusMessage& request) {
        messageQueue.push(request);
        // Add the block to the set of active transactions
        if(request.address != -1 && (request.type == BusMessageType::GetM || request.type == BusMessageType::GetS )) activeTransactions.insert(request.address);
    }

    // Function to check if a transaction is valid
    bool isTransactionValid(const BusMessage& request) {
        // Check if the block is currently involved in an active transaction
        return activeTransactions.find(request.address) == activeTransactions.end();
    }

    // Function to remove a completed transaction
    void removeCompletedTransaction(int blockAddress) {
        // Remove the block from the set of active transactions
        activeTransactions.erase(blockAddress);
    }
};

#endif // BUS_H
