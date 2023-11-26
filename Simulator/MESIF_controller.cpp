// Implements rules for mesif controller 
// Functionally same as MESI, but only 1 forward state to respond to share requests
// Notably F state is clean, it can be dropped
// Implementation is based on description used in Intel Patent US6922756B2

#include "cache_controller.h"
#include "MESIF_controller.h"
#include "bus.h"
#include <cassert>

//Simple Constructor
MESIFController::MESIFController(Cache& cache, Metrics* metrics, int controllerId, Bus& bus, bool debug)
    : CacheController(cache, metrics, controllerId, bus, debug) {
}

//RUN 1x Each "tick"
void MESIFController::processRequest(){
    if (!waitingForResponse && !requestQueue.empty()) {
        // Process the request
        CacheRequest request = requestQueue.front();

        //Enforce an atomic transaction, test if address involved include others
        BusMessage test{
            request.requestedAddress,
            controllerId,
            BusMessageType::NO_MSG
        };
        //Test if transaction keeps atomic transaction
        if(bus.isTransactionValid(test)){
            //test if replacement is needed
            Block * searchBlock = cache.findBlock(request.requestedAddress);
            if(searchBlock){
                processCacheRequest(request);
                requestQueue.pop(); 
            }
            else{ //Check if replacement block can be modified
                Block * toBeReplaced = cache.findReplacementBlock(request.requestedAddress);
                BusMessage test2{
                    toBeReplaced->address,
                    controllerId,
                    BusMessageType::NO_MSG
                };
                if(bus.isTransactionValid(test2)){
                    processCacheRequest(request);
                    requestQueue.pop(); 
                }
            }
        }
    }
}

//RUN up to 1x Each "tick" because it's from Bus
ResponseMessageType MESIFController::processBusMessage(const BusMessage& message) {
    
    std::cout<<"Thread:"<< controllerId <<" Message:"<< busMessageTypeToString(message.type)<< 
        " Process from: "<< message.originThread <<"\n";
    //check state of local block (figure out current state)
    Block * searchBlock = cache.findBlock(message.address);
    if(searchBlock && searchBlock->state != INVALID) std::cout << cacheBlockStateToString(searchBlock->state);
    else std::cout << "block invalid or not in cache";
    
    if(message.originThread == controllerId){//Check if from self (for replacement to be okay/communicated)
        
        if(message.type == BusMessageType::PutM){
            if(searchBlock->state == MODIFIED){
                //Send data to memory if modified
                metrics->total_write_back++;
                metrics->total_msg++;
            }
            std::cout<<" -> INVALID (SELF)\n"; 
            searchBlock->state = INVALID;
            metrics->total_inval++;
        }
    }
    else{
        //If Containing a valid block
        if(searchBlock && searchBlock->state != INVALID){
            switch(message.type){
                case BusMessageType::GetS:
                    if(searchBlock->state == EXCLUSIVE || searchBlock->state == MODIFIED || searchBlock->state == FORWARD){
                        //send to memory and requestor
                        metrics->total_write_back++;
                        //change state to shared (E/M -> S)
                        std::cout<<" -> SHARED \n";
                        searchBlock->state = SHARED;
                        return ResponseMessageType::ACK_CACHE_TO_CACHE;
                    }
                    //Is Shared -> can send data directly
                    else if(searchBlock->state == SHARED) return ResponseMessageType::ACK_CACHE_TO_CACHE;
                    //Note shared does not do anything because it could be another shared requesting
                    //a possible optimization is to include information about the requestor's state in the broadcast
                    break;
                case BusMessageType::GetM:
                    if(
                        searchBlock->state == EXCLUSIVE 
                        || searchBlock->state == MODIFIED
                        || searchBlock->state == FORWARD
                    ){
                        //change state to shared (E/M -> I)
                        std::cout<<"E/M/F -> INVALID \n";
                        searchBlock->state = INVALID;
                        //This response is direct?
                        return ResponseMessageType::ACK_CACHE_TO_CACHE;
                    }
                    //All states must invalidate for write-invalidate
                    std::cout<<"S -> INVALID \n";
                    searchBlock->state = INVALID;
                    return ResponseMessageType::ACK;//send ack direct for invalidating
                    break;
                default:
                    break;
            }
        }
    }
    //Signal good for getM
    if(message.type == BusMessageType::GetM) return ResponseMessageType::ACK;
    //Does not contain block = don't care
    return ResponseMessageType::NO_ACK;
}

//RUN up to 1x Each "tick" 
//Metrics cache-cache do not actually count towards bus comms
//Assumptions that data loads have seperate channels
void MESIFController::processBusResponse(const BusMessage& message, const ResponseMessageType& response) {
    //All responses is to the original requestor
    assert(message.originThread == controllerId);

    //Where is data/ACK from?
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
        bus.removeCompletedTransaction(toBeReplaced->address);
        
        //assert toBeReplaced has invalid
        assert(toBeReplaced->state == CacheBlockState::INVALID);
        if(message.type == BusMessageType::GetM){
            //Perform Replacement (Write I->M)
            std::cout<<"Perform Replacement (Write I->M) \n";
            cache.replaceBlock(message.address, MODIFIED, 0);
        }
        else{
            //Perform Replace (READ I->F/E)
            assert(message.type == BusMessageType::GetS);
            CacheBlockState newState;
            switch(response){
                //Change from MESI, Read miss with hit in cache -> Forward state
                case ResponseMessageType::ACK_CACHE_TO_CACHE:
                    std::cout<<"(I->F) \n";
                    newState = FORWARD;
                    metrics->total_cache_to_cache++;
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
        //States that need to send a message before updating to modified
        if(searchBlock->state == CacheBlockState::SHARED 
        || searchBlock->state == CacheBlockState::FORWARD){
            //FIFO means in update, it still stays in order
            assert(message.type == BusMessageType::GetM); //The only valid
            std::cout<<"(S/F->M) \n";
            searchBlock->state = MODIFIED;
        }
        else{
            assert(searchBlock->state == CacheBlockState::INVALID);
            if(message.type == BusMessageType::GetM){
                //Perform Replacement (Write I->M)
                std::cout<<"(I->M) \n";
                searchBlock->state = MODIFIED;
            }
            else{
                //Perform Replace (READ I->F/E)
                assert(message.type == BusMessageType::GetS);
                CacheBlockState newState;
                switch(response){
                    //Change from MESI, Read miss with hit in cache -> Forward state
                    case ResponseMessageType::ACK_CACHE_TO_CACHE:
                        std::cout<<"(I->F) \n";
                        newState = FORWARD;
                        metrics->total_cache_to_cache++;
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
    bus.removeCompletedTransaction(message.address);
    waitingForResponse = false;
    //May have new request if fail
}

//Is called when there is a cache request to be processed
//Determines if action requires sending information to the bus
void MESIFController::processCacheRequest(const CacheRequest& request) {
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