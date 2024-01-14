//Helper method definitions for bus
#include "bus.h"

//MEthods of bus,h
void Bus::addBusRequest(const BusMessage& request) {
    messageQueue.push(request);
    if (request.address != -1 && (request.type == BusMessageType::GetM || request.type == BusMessageType::GetS || request.type == BusMessageType::Upg))
        activeTransactions.insert(request.address);
}

bool Bus::isTransactionValid(const BusMessage& request) {
    bool notInProgress = (activeTransactions.find(request.address) == activeTransactions.end());
    if (notInProgress)
        busWait = true;
    return notInProgress;
}

void Bus::removeCompletedTransaction(int blockAddress) {
    busWait = false;
    activeTransactions.erase(blockAddress);
}

//Debug Methods
std::string busMessageTypeToString(BusMessageType messageType) {
    switch (messageType) {
        case BusMessageType::NO_MSG:
            return "NO_MSG";
        case BusMessageType::GetS:
            return "GetS";
        case BusMessageType::GetM:
            return "GetM";
        case BusMessageType::Upg:
            return "Upg";
        case BusMessageType::PutS:
            return "PutS";
        case BusMessageType::PutE:
            return "PutE";
        case BusMessageType::PutO:
            return "PutO";
        case BusMessageType::PutM:
            return "PutM";
        default:
            return "Unknown";
    }
};

std::string responseMessageTypeToString(ResponseMessageType responseType) {
    switch (responseType) {
        case ResponseMessageType::NO_ACK:
            return "NO_ACK";
        case ResponseMessageType::ACK:
            return "ACK";
        case ResponseMessageType::ACK_CACHE_TO_CACHE:
            return "ACK_CACHE_TO_CACHE";
        case ResponseMessageType::ACK_DATA_TO_MEM:
            return "ACK_DATA_TO_MEM";
        case ResponseMessageType::ACK_DATA_FROM_MEM:
            return "ACK_DATA_FROM_MEM";
        case ResponseMessageType::ACK_DATA_FROM_MEM_SHRD:
            return "ACK_DATA_FROM_MEM_SHRD";
        case ResponseMessageType::ACK_NO_DATA:
            return "ACK_NO_DATA";
        default:
            return "Unknown";
    }
};

