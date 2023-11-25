#include "cache_controller.h"

CacheController::CacheController(Cache& cache, Metrics* metrics, int id, Bus& bus): cache(cache), metrics(metrics), controllerId(id), bus(bus){};

void CacheController::sendBusMessage(const BusMessage& message){
    bus.addBusRequest(message);
};

void CacheController::enqueueRequest(const CacheRequest& request){
    requestQueue.push(request);
};