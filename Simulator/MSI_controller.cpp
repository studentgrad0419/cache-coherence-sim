// Implements rules for msi controller as described from table 7.8  “7.2 Baseline Snooping Protocol.” Primer on Memory Consistency and Cache Coherence, Second Edition (Nagarajan et al., 2020) pp. 127–127. 

#include "cache_controller.h"
#include "MSI_controller.h"
#include "bus.h"
#include <cassert>

//Simple Constructor
MSIController::MSIController(Cache& cache, Metrics* metrics, int controllerId, Bus& bus, bool debug)
    : CacheController(cache, metrics, controllerId, bus, debug) {
}

//RUN 1x Each "tick", determines if request is allowed then processes it
void MSIController::processRequest(){
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
void MSIController::processCacheRequest(const CacheRequest& request) {
    // You may need to call functions from CacheController or perform additional actions
    Block * searchBlock = cache.findBlock(request.requestedAddress); //Could return I

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
            if(searchBlock->state == SHARED){
                // Create a new bus message for the request
                BusMessage busRequest{
                    request.requestedAddress,
                    controllerId,
                    BusMessageType::GetM
                };
                if(debug) std::cout<<"   Sent GetM\n";
                bus.addBusRequest(busRequest);
                metrics->total_msg++;
                waitingForResponse = true;
            }
            else{
                //Only defined for Modified -> Modified
            }
        }
    }
    //MISS = I state or new block
    else{
        metrics->cacheMetrics[controllerId].cache_miss++;
        //Figure out Invalidate action if needed
        if(!searchBlock){
            //Get Block to be replaced (any param)
            Block * toBeReplaced = cache.findReplacementBlock(request.requestedAddress);
            //Check if it's a valid block
            if(toBeReplaced->state != INVALID){
                if(toBeReplaced->state == SHARED){
                    //silent invalidation (okay to invalidate now)
                    if(debug) std::cout<<"   Silent Invalid: "<<std::hex << toBeReplaced->address << std::dec << "\n";
                    toBeReplaced->state = INVALID;
                    metrics->total_inval++;
                }
                else if(toBeReplaced->state == MODIFIED){
                    //need to issue PutM for this address, after self-putm to send/invalidate
                    BusMessage busRequest{
                        toBeReplaced->address,
                        controllerId,
                        BusMessageType::PutM
                    };
                    if(debug) std::cout<<"   Sent PutM\n";
                    bus.addBusRequest(busRequest);
                    metrics->total_msg++;
                }
            }
        }
        //Figure out request
        //Case Write
        if(request.readWriteBit){
            // Create a new bus message for the request
            BusMessage busRequest{
                request.requestedAddress,
                controllerId,
                BusMessageType::GetM
            };
            if(debug) std::cout<<"   Sent GetM\n";
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
            if(debug) std::cout<<"   Sent GetS\n";
            bus.addBusRequest(busRequest);
            metrics->total_msg++;
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
ResponseMessageType MSIController::processBusMessage(const BusMessage& message) {
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
    
    //Check if from self (for replacement to be okay/communicated)
    if(message.originThread == controllerId){
        //if(!searchBlock) throw std::runtime_error("Error: No block found for a self-originated message.");
        //Check case = PUT_M
        if(message.type == BusMessageType::PutM){
            if(searchBlock->state == MODIFIED){
                //Send data to memory if modified
                metrics->total_write_back++;
                if(debug) std::cout << "  MEMORY WRITTEN  ";
                metrics->total_msg++;
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
        //If Containing a valid block, request from other
        if(searchBlock && searchBlock->state != INVALID){
            switch(message.type){
                case BusMessageType::GetS:
                    if(searchBlock->state == MODIFIED){
                        //send to memory and requestor
                        metrics->total_write_back++;
                        if(debug) std::cout << "  MEMORY WRITTEN  ";
                        searchBlock->state = SHARED;
                        returnVal = ResponseMessageType::ACK_CACHE_TO_CACHE;
                    }
                    break;
                case BusMessageType::GetM:
                    if(searchBlock->state == MODIFIED){
                        searchBlock->state = INVALID;
                        metrics->total_inval++;
                        returnVal = ResponseMessageType::ACK_CACHE_TO_CACHE;
                    }
                    else{
                        //All states must invalidate for write-invalidate
                        searchBlock->state = INVALID;
                        metrics->total_inval++;
                        returnVal = ResponseMessageType::ACK;//send ack direct for invalidating
                    }
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
    if(returnVal == ResponseMessageType::NO_ACK && message.type == BusMessageType::GetM) returnVal = ResponseMessageType::ACK;
    //Does not contain block = don't care
    return returnVal;
}

//RUN up to 1x Each "tick" 
//Metrics cache-cache do not actually count towards bus comms
//Assumptions that data loads have seperate channels
//This is for transitions that need data
void MSIController::processBusResponse(const BusMessage& message, const ResponseMessageType& response) {
    
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

    //Classify ACK
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

    //REPLACEMENT HAS OCCURED, ONE BLOCK IS RELEASED ALREADY, CONTINUE WITH REPLACEMENT
    if(!searchBlock){
        //Logic means first replaced (also called victim block)
        Block * toBeReplaced = cache.findReplacementBlock(message.address);
        bus.removeCompletedTransaction(toBeReplaced->address);
        
        //assert toBeReplaced has invalid
        assert(toBeReplaced->state == CacheBlockState::INVALID);
        if(message.type == BusMessageType::GetM){
            //Creates new block by FIFO replacement
            cache.replaceBlock(message.address, MODIFIED, 0);
        }
        else{
            //Perform Replace (READ I->S/E)
            assert(message.type == BusMessageType::GetS);
            CacheBlockState newState;
            if(
                response == ResponseMessageType::ACK_CACHE_TO_CACHE
                || response == ResponseMessageType::ACK_DATA_FROM_MEM){
                newState = SHARED;
            }
            //Creates new block by FIFO replacement
            cache.replaceBlock(message.address, newState, 0);
        }
    }
    //UPDATING AN EXISTING ENTRY IN CACHE
    else{
        //this is S or I with matching address and still inside cache
        if(searchBlock->state == CacheBlockState::SHARED){
            //FIFO means in update, it still stays in order
            assert(message.type == BusMessageType::GetM); //The only valid option
            searchBlock->state = MODIFIED;
        }
        else{
            assert(searchBlock->state == CacheBlockState::INVALID);
            if(message.type == BusMessageType::GetM){
                //update existing block
                searchBlock->state = MODIFIED;
            }
            else{
                //Perform Replace (READ I->S/E)
                assert(message.type == BusMessageType::GetS);
                CacheBlockState newState;
                if(
                    response == ResponseMessageType::ACK_CACHE_TO_CACHE
                 || response == ResponseMessageType::ACK_DATA_FROM_MEM){
                    newState = SHARED;
                }
                else assert(0);
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