// Cache controller defines what is needed to handle cache requests
// that includes access to cache, instructions(as a queue), and a bus
// also defines Cache Requests from thread 
#ifndef CACHE_CONTROLLER_H
#define CACHE_CONTROLLER_H

#include <iostream>
#include "bus.h"
#include "cache.h"
#include "metrics.h"
#include <queue>

// Struct representing a cache request
struct CacheRequest {
    int time;
    int thread;
    int requestedAddress; // cache line-aligned
    int readWriteBit;
};

class CacheController {
public:
    Cache& cache;
    Metrics* metrics;
    int controllerId;
    Bus& bus;
    bool debug = false;
    bool waitingForResponse = false;
    bool hasMessageHeld = false;
    std::queue<BusMessage> heldMessages;
    std::queue<CacheRequest> requestQueue;

    CacheController(Cache& cache, Metrics* metrics, int id, Bus& bus, bool debug);

    // Function to enqueue a cache request
    void enqueueRequest(const CacheRequest& request);

    // Function to process a cache request
    virtual void processRequest()=0;

    // Function to process a bus message
    virtual ResponseMessageType processBusMessage(const BusMessage& message)=0;

    // Function to process a response (for simulation purposes, the original message and response type is used)
    virtual void processBusResponse(const BusMessage& message, const ResponseMessageType& response)=0;

    // Function to send a bus message
    void sendBusMessage(const BusMessage& message);

    // Function to process a cache request (replace with your actual logic)
    virtual void processCacheRequest(const CacheRequest& request)=0;
};

#endif  // CACHE_CONTROLLER_H