//MSI controller extends Cache Controller
//Implements MESI Protocol
#ifndef MSI_CONTROLLER_H
#define MSI_CONTROLLER_H

#include "cache_controller.h"

class MSIController : public CacheController {
public:
    MSIController(Cache& cache, Metrics* metrics, int id, Bus& bus, bool debug);

public:
    // Override functions to implement MSI protocol
    void processRequest() override;
    ResponseMessageType processBusMessage(const BusMessage& message) override;
    void processBusResponse(const BusMessage& message, const ResponseMessageType& response) override;
    void processCacheRequest(const CacheRequest& request) override;
};

#endif  // MSI_CONTROLLER_H