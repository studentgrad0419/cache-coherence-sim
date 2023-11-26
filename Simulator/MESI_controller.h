//MESI controller extends Cache Controller
//Implements MESI Protocol
#ifndef MESI_CONTROLLER_H
#define MESI_CONTROLLER_H

#include "cache_controller.h"

class MESIController : public CacheController {
public:
    MESIController(Cache& cache, Metrics* metrics, int id, Bus& bus, bool debug);

public:
    // Override functions to implement MESI protocol
    void processRequest() override;
    ResponseMessageType processBusMessage(const BusMessage& message) override;
    void processBusResponse(const BusMessage& message, const ResponseMessageType& response) override;
    void processCacheRequest(const CacheRequest& request) override;
};

#endif  // MESI_CONTROLLER_H