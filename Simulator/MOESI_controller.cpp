// Implements rules for moesi controller as described by state transition diagram
// State transitions decribed as actions requested by CPU and Messages from Bus
// This implementation is based on the state diagram AMD64 Architecture Programmer’s Manual Volume 2: System Programming
// Language used in comments will reflect the descriptions used in the literature on page 170
// For other reference, paper MOESIL: A Cache Coherency Protocol for Locked Mixed Criticality L1 Data Cache (S. Nair et. al)

#include "cache_controller.h"
#include "MOESI_controller.h"
#include "bus.h"
#include <cassert>

//Simple Constructor
MOESIController::MOESIController(Cache& cache, Metrics* metrics, int controllerId, Bus& bus, bool debug)
    : CacheController(cache, metrics, controllerId, bus, debug) {
}

// Handling Processor Requests

//RUN 1x Each "tick"
void MOESIController::processRequest(){
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
void MOESIController::processCacheRequest(const CacheRequest& request) {
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
        //If Write, need to "probe write hit"
        if(request.readWriteBit){
            // if(searchBlock->state == SHARED || searchBlock->state == OWNED){
            if(searchBlock->state == SHARED){
                // Create a new bus message for "probe write hit" 
                BusMessage busRequest{
                    request.requestedAddress,
                    controllerId,
                    BusMessageType::GetM
                };
                if(debug) std::cout<<"   Sent GetM\n";
                bus.addBusRequest(busRequest);
                waitingForResponse = true; 
                // For consistency, want to make sure others update before it can move to modified 
            }
            else if(searchBlock->state == EXCLUSIVE || searchBlock->state == OWNED){
                //silent transition to E->M because it's the only and most recent copy
                searchBlock->state = MODIFIED;
            }
            //Else M->M is fine
        }
    }
    //MISS = I state or new block
    else{
        metrics->cacheMetrics[controllerId].cache_miss++;   
        //Figure out if replacement action is needed (WBINV)
        if(!searchBlock){
            //Get Block to be replaced (any param)
            Block * toBeReplaced = cache.findReplacementBlock(request.requestedAddress);
            //Check if it's a valid block
            if(toBeReplaced->state != INVALID){
                if(toBeReplaced->state == SHARED){
                    //silent invalidation (okay to invalidate now)
                    //noted shared will never transition to exclusive
                    toBeReplaced->state = INVALID;
                    if(debug) std::cout<<"   Silent Invalid: "<<std::hex << toBeReplaced->address << std::dec << "\n";
                    metrics->total_inval++;
                }
                else if(toBeReplaced->state == EXCLUSIVE || toBeReplaced->state == MODIFIED || toBeReplaced->state == OWNED){
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
        //Figure out the requests
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

//Handling Bus MESSAGES & ACKS

//RUN up to 1x Each "tick" because it's from Bus
ResponseMessageType MOESIController::processBusMessage(const BusMessage& message) {
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
            //The only states that can differ from memory is S*/M/O
            //S is only different if there exists an O S can safely be Invalidated silently
            //Thus we only need to write back O/M when invalid via replacement
            if(searchBlock->state == MODIFIED || searchBlock->state == OWNED){
                //Send data to memory if modified
                if(debug) std::cout << "  MEMORY WRITTEN  ";
                returnVal = ResponseMessageType::ACK_DATA_TO_MEM;
            } 
            else{
                returnVal = ResponseMessageType::ACK;
            }
            // E/M/O/S -> I
            searchBlock->state = INVALID;
            metrics->total_inval++;
        }
    }
    else{
        //If Containing a valid block
        if(searchBlock && searchBlock->state != INVALID){
            switch(message.type){
                case BusMessageType::GetS: 
                    //KEY difference from MESI, Modified does not need to write back to mem
                    if(searchBlock->state == MODIFIED){
                        //send only to requestor
                        searchBlock->state = OWNED;
                        returnVal = ResponseMessageType::ACK_CACHE_TO_CACHE;
                    }
                    else if(searchBlock->state == EXCLUSIVE){
                        searchBlock->state = SHARED;
                        returnVal = ResponseMessageType::ACK_CACHE_TO_CACHE;
                    }
                    //Has a valid copy
                    else if(searchBlock->state == OWNED){
                        //send only to requestor, no state change
                        //For simplification of simulation, if Owned exist in any of the caches, 
                        // presume OWNED is the data being passed as all caches are expected to snoop
                        returnVal = ResponseMessageType::ACK_CACHE_TO_CACHE;
                    }
                    break;
                case BusMessageType::GetM:
                    //Another point here
                    //Due to O not writting back to memory, S that reads may have data that is more recent than memory
                    //S will not be sharing to follow the MESI implementation
                    //Note S GetM with other S would cause more bandwitdh usage if not needed.
                    if(
                        searchBlock->state == EXCLUSIVE 
                        || searchBlock->state == MODIFIED
                        || searchBlock->state == OWNED
                    ){
                        //change state to shared (E/M/O -> I)
                        searchBlock->state = INVALID;
                        metrics->total_inval++;
                        returnVal =  ResponseMessageType::ACK_CACHE_TO_CACHE;
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
void MOESIController::processBusResponse(const BusMessage& message, const ResponseMessageType& response) {
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
    
    
    //REPLACEMENT HAS OCCURED, ONE BLOCK IS RELEASED ALREADY, CONTINUE WITH REPLACEMENT
    if(!searchBlock){
        //Logic means first replaced
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
            switch(response){
                case ResponseMessageType::ACK_CACHE_TO_CACHE://Read Miss Shared
                    newState = SHARED;
                    break;
                case ResponseMessageType::ACK_DATA_FROM_MEM://Read-miss exclusive
                    newState = EXCLUSIVE;
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
        //NOTE updating with FIFO replacement does not update order, that would be LRU
        // if(searchBlock->state == CacheBlockState::SHARED || searchBlock->state == CacheBlockState::OWNED){
        if(searchBlock->state == CacheBlockState::SHARED || searchBlock->state == CacheBlockState::OWNED || searchBlock->state == CacheBlockState::EXCLUSIVE){
            //This means that Probe-Write has been respected and all others have invalidated
            assert(message.type == BusMessageType::GetM);
            searchBlock->state = MODIFIED;
        }
        else{
            //Invalid into new state, like replacement but updating old entry
            assert(searchBlock->state == CacheBlockState::INVALID);
            if(message.type == BusMessageType::GetM){
                //update existing block
                searchBlock->state = MODIFIED;
            }
            else{
                //Perform Update (READ I->S/E)
                assert(message.type == BusMessageType::GetS);
                CacheBlockState newState;
                switch(response){
                    case ResponseMessageType::ACK_CACHE_TO_CACHE:
                        newState = SHARED;
                        break;
                    case ResponseMessageType::ACK_DATA_FROM_MEM:
                        newState = EXCLUSIVE;
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