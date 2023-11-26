//MOESI controller extends Cache Controller
//Implements MOESI Protocol
#ifndef MOESI_CONTROLLER_H
#define MOESI_CONTROLLER_H

#include "cache_controller.h"

class MOESIController : public CacheController {
public:
    MOESIController(Cache& cache, Metrics* metrics, int id, Bus& bus, bool debug);

public:
    // Override functions to implement MESI protocol
    void processRequest() override;
    ResponseMessageType processBusMessage(const BusMessage& message) override;
    void processBusResponse(const BusMessage& message, const ResponseMessageType& response) override;
    void processCacheRequest(const CacheRequest& request) override;
};

#endif  // MOESI_CONTROLLER_H