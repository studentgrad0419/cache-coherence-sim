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

//Is called when there is a cache request to be processed
//Determines if action requires sending information to the bus
void MESIFController::processCacheRequest(const CacheRequest& request) {
    // You may need to call functions from CacheController or perform additional actions
    Block * searchBlock = cache.findBlock(request.requestedAddress); //finds non-I
    
    if(debug){
       std::cout<<" Thread:"<< controllerId <<" Process CPU Req: "<< ((request.readWriteBit == 0) ? " READ " : " WRITE ") << 
        " For Block: "<< std::hex << request.requestedAddress << std::dec <<"   ";
        
        if(searchBlock) std::cout<< "   Initial state: " << cacheBlockStateToString(searchBlock->state); 
        else std::cout<< "   Block not in L1\n";
    }

    //HIT 
    if(searchBlock && searchBlock->state != INVALID){
        metrics->cacheMetrics[controllerId].cache_hit++;
        //If Write, need to check state
        if(request.readWriteBit){
            if(searchBlock->state == SHARED || searchBlock->state == FORWARD){
                // Create a new bus message for the request
                BusMessage busRequest{
                    request.requestedAddress,
                    controllerId,
                    BusMessageType::Upg
                };
                if(debug) std::cout<<"   Sent Upg\n";
                bus.addBusRequest(busRequest);
                waitingForResponse = true;
                //searchBlock->state = MODIFIED;
            }
            else if(searchBlock->state == EXCLUSIVE){
                //silent transition to E->M
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
                if(toBeReplaced->state == SHARED || toBeReplaced->state == FORWARD || toBeReplaced->state == EXCLUSIVE){
                    //silent invalidation (okay to invalidate now, FORWARD Property)
                    toBeReplaced->state = INVALID;
                    if(debug) std::cout<<"   Silent Invalid: "<<std::hex << toBeReplaced->address << std::dec << "\n";
                    metrics->total_inval++;
                }
                else if(toBeReplaced->state == EXCLUSIVE 
                || toBeReplaced->state == MODIFIED 
                ){
                    //need to issue PutM for this address, after self-putm to send/invalidate
                    BusMessage busRequest{
                        toBeReplaced->address,
                        controllerId,
                        BusMessageType::PutM
                    };
                    if(debug) std::cout<<"   Sent PutM (WBINV)\n";
                    bus.addBusRequest(busRequest);
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
            if(debug) std::cout<<"   Sent GetM (Write Miss)\n";
            bus.addBusRequest(busRequest);
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
            if(debug) std::cout<<"   Sent GetS (Read Miss)\n";
            bus.addBusRequest(busRequest);
            waitingForResponse = true;
        }
    }
    if(debug){
        if(waitingForResponse) std::cout<< "   Block update awaits response\n";
        else if(searchBlock) std::cout<< "   After state: " << cacheBlockStateToString(searchBlock->state) << '\n'; 
        else std::cout<< "   Block not in L1\n";
    }
}

//RUN up to 1x Each "tick" because it's from Bus
ResponseMessageType MESIFController::processBusMessage(const BusMessage& message) {
    //check state of local block (figure out current state)
    Block * searchBlock = cache.findBlock(message.address);
    ResponseMessageType returnVal = ResponseMessageType::NO_ACK;
    if(debug){
       std::cout<<" Thread:"<< controllerId <<" Snooped: "<< busMessageTypeToString(message.type)<< 
        " from: "<< message.originThread << " For Block: "<< std::hex << message.address << std::dec <<"   ";;
        if(searchBlock){
            std::cout<< "   Initial state: " << cacheBlockStateToString(searchBlock->state); 
        }
        else std::cout<< "   Block not in L1\n";
        
    }
    
    if(message.originThread == controllerId){//Check if from self (for replacement to be okay/communicated)
        
        if(message.type == BusMessageType::PutM){
            if(searchBlock->state == MODIFIED){
                //Send data to memory if modified
                if(debug) std::cout << "  MEMORY WRITTEN  ";
                returnVal = ResponseMessageType::ACK_DATA_TO_MEM;
            } 
            else{
                returnVal = ResponseMessageType::ACK;
            } 
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
                        if(debug) std::cout << "  MEMORY WRITTEN  ";
                        //change state to shared (E/M -> S);
                        metrics->total_write_back++;//panick removed ealier
                        searchBlock->state = SHARED;
                        returnVal = ResponseMessageType::ACK_CACHE_TO_CACHE;
                    }
                    //Is FORWARD -> can send data directly
                    else if(searchBlock ->state == FORWARD) {
                        returnVal = ResponseMessageType::ACK_CACHE_TO_CACHE;
                        searchBlock->state = SHARED;
                    }
                    //no shared state passing becuase of forward / m otherwise fetch from mem
                    //a possible optimization is to include information about the requestor's state in the broadcast

                    //Not part of spec, for purpose of tracking shared for exclusive state
                    else if(searchBlock->state == SHARED) returnVal = ResponseMessageType::ACK_DATA_FROM_MEM_SHRD;
                    break;
                case BusMessageType::GetM:
                    if(
                        searchBlock->state == EXCLUSIVE 
                        || searchBlock->state == MODIFIED
                        || searchBlock->state == FORWARD
                    ){
                        //change state to shared (E/M -> I)
                        searchBlock->state = INVALID;
                        metrics->total_inval++;
                        //This response is direct?
                        returnVal = ResponseMessageType::ACK_CACHE_TO_CACHE;
                    }
                    else{
                        //All states must invalidate for write-invalidate
                        searchBlock->state = INVALID;
                        metrics->total_inval++;
                        //returnVal = ResponseMessageType::ACK;//send ack direct for invalidating
                    }
                    break;
                case BusMessageType::Upg:
                    //Part of Write-Invalidate
                    searchBlock->state = INVALID;
                    metrics->total_inval++;
                    //returnVal = ResponseMessageType::NO_ACK;//ACK is not needed
                    break;
                default:
                    break;
            }
        }
    }

    if(debug){
        if(searchBlock){
            std::cout<< "   After state: " << cacheBlockStateToString(searchBlock->state) << '\n'; 
        }
    }
    //Signal good for getM 
    //if(returnVal == ResponseMessageType::NO_ACK && message.type == BusMessageType::GetM) returnVal = ResponseMessageType::ACK;
    //Does not contain block = don't care
    return returnVal;
}

//RUN up to 1x Each "tick" 
//Metrics cache-cache do not actually count towards bus comms
//Assumptions that data loads have seperate channels
void MESIFController::processBusResponse(const BusMessage& message, const ResponseMessageType& response) {
    //If search self and have or need to update?
    Block * searchBlock = cache.findBlock(message.address);

    if(debug){
       std::cout<<" Thread:"<< controllerId <<" Recieved Confirmation: "<< responseMessageTypeToString(response)<< 
        " For Block: "<< std::hex << message.address << std::dec <<"   ";
        
        if(searchBlock) std::cout<< "   Initial state: " << cacheBlockStateToString(searchBlock->state); 
        else std::cout<< "   Block not in L1";
        
    }
    //All responses is to the original requestor
    assert(message.originThread == controllerId);

    if(message.type == BusMessageType::Upg){
        assert(searchBlock);
        searchBlock->state = MODIFIED;
    }
    //REPLACEMENT HAS OCCURED, ONE BLOCK IS RELEASED ALREADY, CONTINUE WITH REPLACEMENT
    else if(!searchBlock){
        //Logic means first replaced
        Block * toBeReplaced = cache.findReplacementBlock(message.address);
        bus.removeCompletedTransaction(toBeReplaced->address);
        
        //assert toBeReplaced has invalid
        assert(toBeReplaced->state == CacheBlockState::INVALID);
        if(message.type == BusMessageType::GetM){
            //Perform Replacement (Write I->M)
            cache.replaceBlock(message.address, MODIFIED, 0);
        }
        else{
            //Perform Replace (READ I->F/E)
            assert(message.type == BusMessageType::GetS);
            CacheBlockState newState;
            switch(response){
                //Change from MESI, Read miss with hit in cache -> Forward state
                case ResponseMessageType::ACK_CACHE_TO_CACHE:
                    newState = FORWARD;
                    break;
                case ResponseMessageType::ACK_DATA_FROM_MEM:
                    newState = EXCLUSIVE;
                    break;
                case ResponseMessageType::ACK_DATA_FROM_MEM_SHRD:
                    newState = SHARED;
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
            searchBlock->state = MODIFIED;
        }
        else{
            assert(searchBlock->state == CacheBlockState::INVALID);
            if(message.type == BusMessageType::GetM){
                //Perform Replacement (Write I->M)
                searchBlock->state = MODIFIED;
            }
            else{
                //Perform Replace (READ I->F/E)
                assert(message.type == BusMessageType::GetS);
                CacheBlockState newState;
                switch(response){
                    //Change from MESI, Read miss with hit in cache -> Forward state
                    case ResponseMessageType::ACK_CACHE_TO_CACHE:
                        newState = FORWARD;
                        break;
                    case ResponseMessageType::ACK_DATA_FROM_MEM:
                        newState = EXCLUSIVE;
                        break;
                    case ResponseMessageType::ACK_DATA_FROM_MEM_SHRD:
                        newState = SHARED;
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
    if(debug){
        Block * searchBlock = cache.findBlock(message.address);
        if(searchBlock) std::cout<< "   After state: " << cacheBlockStateToString(searchBlock->state) << '\n'; 
    }
    //Anything with a response means the request is completed
    bus.removeCompletedTransaction(message.address);
    waitingForResponse = false;
    //May have new request if fail
}
