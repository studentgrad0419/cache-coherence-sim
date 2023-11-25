// Defines rules for mesi controller

#include "cache_controller.h"
#include "mesi_controller.h"
#include "bus.h"
#include <cassert>
#include <string>

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
        case ResponseMessageType::ACK_NO_DATA:
            return "ACK_NO_DATA";
        default:
            return "Unknown";
    }
};

//Simple Constructor
MESIController::MESIController(Cache& cache, Metrics* metrics, int controllerId, Bus& bus)
    : CacheController(cache, metrics, controllerId, bus) {
}

//RUN 1x Each "tick"
void MESIController::processRequest(){
    if (!waitingForResponse && !requestQueue.empty()) {
        // Process the request
        CacheRequest request = requestQueue.front();
        std::cout<<"Thread:"<< request.thread <<" Request Processed: "<< request.readWriteBit <<"\n";
        processCacheRequest(request);
        requestQueue.pop();
    }
}

//RUN up to 1x Each "tick" because it's from Bus
ResponseMessageType MESIController::processBusMessage(const BusMessage& message) {
    
    std::cout<<"Thread:"<< controllerId <<" Message:"<< busMessageTypeToString(message.type)<< 
        " Process from: "<< message.originThread <<"\n";
    //check state of local block (figure out current state)
    Block * searchBlock = cache.findBlock(message.address);

    //Check if from self (for replacement to be okay/communicated)
    if(message.originThread == controllerId){
        //if(!searchBlock) throw std::runtime_error("Error: No block found for a self-originated message.");
        //Check case = PUT_M
        if(message.type == BusMessageType::PutM){
            if(searchBlock->state == MODIFIED){
                //Send data to memory if modified
                metrics->total_write_back++;
                metrics->total_msg++;
            }
            // E/M -> I
            std::cout<<"E/M -> INVALID (SELF)\n"; 
            searchBlock->state = INVALID;
            metrics->total_inval++;
        }
    }
    else{
        //If Containing a valid block
        if(searchBlock && searchBlock->state != INVALID){
            switch(message.type){
                case BusMessageType::GetS:
                    if(searchBlock->state == EXCLUSIVE || searchBlock->state == MODIFIED){
                        //send to memory and requestor
                        metrics->total_write_back++;
                        //change state to shared (E/M -> S)
                        std::cout<<"E/M -> SHARED \n";
                        searchBlock->state = SHARED;
                        return ResponseMessageType::ACK_CACHE_TO_CACHE;
                    }
                    //Note shared does not do anything because it could be another shared requesting
                    //Possible optimization is to 
                    break;
                case BusMessageType::GetM:
                    if(
                        searchBlock->state == EXCLUSIVE 
                        || searchBlock->state == MODIFIED
                    ){
                        //change state to shared (E/M -> I)
                        std::cout<<"E/M -> INVALID \n";
                        searchBlock->state = INVALID;
                        //S->S, also can respond/give data
                        return ResponseMessageType::ACK_CACHE_TO_CACHE;
                    }
                    break;
                default:
                    break;
            }
        }
    }
    //Does not contain block = don't care
    return ResponseMessageType::NO_ACK;
}

//RUN up to 1x Each "tick" 
//Metrics cache-cache do not actually count towards bus comms
//Assumptions that large loads have seperate channels
void MESIController::processBusResponse(const BusMessage& message, const ResponseMessageType& response) {
    //All responses is to the original requestor
    //Relevant transitions in MESI: I->S, I->E, I->M, S->M
    //Where is data/ACK from?
    assert(message.originThread == controllerId);
    switch(response){
        case ResponseMessageType::ACK_CACHE_TO_CACHE:
            //Data from Cache
            metrics->total_cache_to_cache++;
            break;
        case ResponseMessageType::ACK_DATA_FROM_MEM:
            //Data from Memory
            metrics->total_msg++;
            metrics->total_read_mem++;
            break;
        default:
            assert(0);//Error
            break;
    }
    std::cout<<"Thread: "<< controllerId << " processing bus response: "
        << responseMessageTypeToString(response) << " from " << busMessageTypeToString(message.type) <<" for: "<< message.address << "\n";

    //If search self and have or need to update?
    Block * searchBlock = cache.findBlock(message.address);
    //REPLACEMENT HAS OCCURED, ONE BLOCK IS RELEASED ALREADY, CONTINUE WITH REPLACEMENT
    if(!searchBlock){
        //Logic means first replaced
        Block * toBeReplaced = cache.findReplacementBlock(message.address);
        
        //assert toBeReplaced has invalid
        assert(toBeReplaced->state == CacheBlockState::INVALID);
        //assert message.type == BusMessageType::GetM
        if(message.type == BusMessageType::GetM){
            //Perform Replacement (Write I->M)
            std::cout<<"Perform Replacement (Write I->M) \n";
            cache.replaceBlock(message.address, MODIFIED, 0);
        }
        else{
            //Perform Replace (READ I->S/E)
            assert(message.type == BusMessageType::GetS);
            CacheBlockState newState;
            switch(response){
                case ResponseMessageType::ACK_CACHE_TO_CACHE:
                    std::cout<<"(I->S) \n";
                    newState = SHARED;
                    break;
                case ResponseMessageType::ACK_DATA_FROM_MEM:
                    std::cout<<"(I->E) \n";
                    newState = EXCLUSIVE;
                    metrics->total_msg++;
                    break;
                default:
                    assert(0);//Error
                    break;
            }
            //Perform Replacement
            cache.replaceBlock(message.address, newState, 0);
        }
    }
    //UPDATING AN EXISTING ENTRY IN CACHE
    else{
        //this is S or previously I and still inside cache
        if(searchBlock->state == CacheBlockState::SHARED){
            //FIFO means in update, it still stays in order
            assert(message.type == BusMessageType::GetM); //The only valid
            // S->M
            std::cout<<"(S->M) \n";
            searchBlock->state = MODIFIED;
        }
        else{
            assert(searchBlock->state == CacheBlockState::INVALID);
            if(message.type == BusMessageType::GetM){
                //Perform Replacement (Write I->M)
                std::cout<<"(I->M) \n";
                cache.replaceBlock(message.address, MODIFIED, 0);
            }
            else{
                //Perform Replace (READ I->S/E)
                assert(message.type == BusMessageType::GetS);
                CacheBlockState newState;
                switch(response){
                    case ResponseMessageType::ACK_CACHE_TO_CACHE:
                        std::cout<<"(I->S) \n";
                        newState = SHARED;
                        break;
                    case ResponseMessageType::ACK_DATA_FROM_MEM:
                        std::cout<<"(I->E) \n";
                        newState = EXCLUSIVE;
                        metrics->total_msg++;
                        break;
                    default:
                        assert(0);//Error
                        break;
                }
                //Update old entry
                searchBlock->state = newState;
            }
        }
    }
    //Anything with a response means the request is completed
    waitingForResponse = false;
    //May have new request if fail
}

//Is called when there is a cache request to be processed
//Determines if action requires sending information to the bus
void MESIController::processCacheRequest(const CacheRequest& request) {
    // Implement MESI-specific cache request handling
    // You may need to call functions from CacheController or perform additional actions
    std::cout<<"Thread: "<< controllerId << "\n";
    Block * searchBlock = cache.findBlock(request.requestedAddress); //finds non-I
    //HIT 
    if(searchBlock && searchBlock->state != INVALID){
        metrics->cacheMetrics[controllerId].cache_hit++;
        //If Write, need to check state
        if(request.readWriteBit){
            if(searchBlock->state == SHARED){
                // Create a new bus message for the request
                BusMessage busRequest{
                    request.requestedAddress,
                    controllerId,
                    BusMessageType::GetM
                };
                std::cout<<" Sent GetM to write";
                bus.addBusRequest(busRequest);
                metrics->total_msg++;
                waitingForResponse = true;
            }
            else if(searchBlock->state == EXCLUSIVE){
                //silent transition to E->M
                std::cout<<" E->M \n";
                searchBlock->state = MODIFIED;
            }
        }
    }
    //MISS = I state or new block
    else{
        metrics->cacheMetrics[controllerId].cache_miss++;
        //Figure out if replacement action is needed (message sent before allocate)
        if(!searchBlock){
            //Get Block to be replaced (any param)
            Block * toBeReplaced = cache.findReplacementBlock(request.requestedAddress);
            //Check if it's a valid block
            if(toBeReplaced->state != INVALID){
                if(toBeReplaced->state == SHARED){
                    //silent invalidation (okay to invalidate now)
                    std::cout<<" S->I cache prep";
                    toBeReplaced->state = INVALID;
                    metrics->total_inval++;
                }
                else if(toBeReplaced->state == EXCLUSIVE || toBeReplaced->state == MODIFIED){
                    //need to issue PutM for this address, after self-putm to send/invalidate
                    BusMessage busRequest{
                        toBeReplaced->address,
                        controllerId,
                        BusMessageType::PutM
                    };
                    std::cout<<" E/M -> PutM cache prep ";
                    bus.addBusRequest(busRequest);
                    metrics->total_msg++;
                }
            }
        }
        //Figure out other request
        //Case Write
        if(request.readWriteBit){
            // Create a new bus message for the request
            BusMessage busRequest{
                request.requestedAddress,
                controllerId,
                BusMessageType::GetM
            };
            std::cout<<" Sent GetM to fetch write \n";
            bus.addBusRequest(busRequest);
            metrics->total_msg++;
            waitingForResponse = true;
            
        }
        //Case Read
        else{
            // Create a new bus message for the request
            BusMessage busRequest{
                request.requestedAddress,
                controllerId,
                BusMessageType::GetS
            };
            std::cout<<" Sent GetS to fetch read \n";
            bus.addBusRequest(busRequest);
            metrics->total_msg++;
            waitingForResponse = true;
        }
    }
}