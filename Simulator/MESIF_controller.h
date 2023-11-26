//MESIF controller extends Cache Controller
//Implements MESIF Protocol
#ifndef MESIF_CONTROLLER_H
#define MESIF_CONTROLLER_H

#include "cache_controller.h"

class MESIFController : public CacheController {
public:
    MESIFController(Cache& cache, Metrics* metrics, int id, Bus& bus, bool debug);

public:
    // Override functions to implement MESIF protocol
    void processRequest() override;
    ResponseMessageType processBusMessage(const BusMessage& message) override;
    void processBusResponse(const BusMessage& message, const ResponseMessageType& response) override;
    void processCacheRequest(const CacheRequest& request) override;
};

#endif  // MESIF_CONTROLLER_H