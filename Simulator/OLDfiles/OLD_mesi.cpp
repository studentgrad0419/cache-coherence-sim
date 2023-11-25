//Run the simulation using the MESI protocol and given parameters
//Based on MESI snooping protocol
//Using an atomic request and atomic transaction property
//This means if a cache is in a "transient state" it should not be modified until a response from the request in the bus is complete.
//So when two "requests" occur at the same time, the "bus" enforces an order (in this case, the order in which they are read from the file)
//Then logically for metrics, it will behave as if processed sequentially (though a real execution would see interleaving of executions)
#include "cache.h"
#include "metrics.h"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <string>

void processSnoopyMemoryTraceLine(const char* line, std::vector<Cache>& caches, Metrics& metric, int block_size);

void initialize_metrics(Metrics* metric, int associativity, int block_size, int num_cache, int num_blocks){
    //Initialize metrics
    metric->total_msg = 0;
    metric->total_ack = 0;
    metric->total_inval = 0;
    metric->total_write_back = 0;
    metric->total_read_mem = 0;
    metric->total_cache_to_cache = 0;

    for (int i = 0; i < num_cache; ++i) {
        // Initialize individual cache metrics
        metric->cacheMetrics[i].bytes_per_set = associativity * block_size;
        metric->cacheMetrics[i].bytes_per_block = block_size;
        metric->cacheMetrics[i].bytes_total = num_blocks * block_size;
        metric->cacheMetrics[i].cache_miss = 0;
        metric->cacheMetrics[i].cache_hit = 0;
    }
}

void MESI_SIM(char* filename, Metrics* metric, int associativity, int block_size, int num_cache, int num_blocks, int isSnoop) {
    // Create a vector of caches (one cache per thread)
    std::vector<Cache> caches;
    for (int i = 0; i < num_cache; ++i) {
        //size of cache = total number of blocks
        caches.emplace_back(num_blocks, associativity);
    }
    std::cout << "Num sets in a cache = "<< caches[0].numSets<<"\n";

    initialize_metrics( metric, associativity, block_size,num_cache,num_blocks);

    // Open the memory trace file
    std::ifstream inputFile(filename);
    if (!inputFile.is_open()) {
        std::cerr << "Error: Unable to open file " << filename << std::endl;
        return;
    }

    // Process each line in the memory trace file
    std::string line;
    while (std::getline(inputFile, line)) {
        // Process the memory trace line
        processSnoopyMemoryTraceLine(line.c_str(), caches, *metric, block_size);
    }

    // Close the input file
    inputFile.close();
}

void mesiSnoopyHit(std::vector<Cache>& caches, Metrics& metric, Block * searchBlock, int requestedAddress, int thread, int readWriteBit, bool debug = false){
    //Hit
    metric.cacheMetrics[thread].cache_hit++;
    if(debug) std::cout<< "Cache hit in thread:"<<thread << '\n';
    //If Write, need to check state
    if(readWriteBit){
        if(searchBlock->state == CacheBlockState::SHARED){
            //Issue BusUpgr (Write-invalidate)
            if(debug) std::cout<< "  Sent out write-invalidate\n" ;
            metric.total_msg++;
            //Check if other cache contain the block
            for(int currThread = 0; currThread < caches.size(); currThread++){
                if(currThread != thread){
                    Block * otherBlock = caches[currThread].findBlock(requestedAddress);
                    if(otherBlock){
                        //Perform Write-invalidate
                        if(debug) std::cout<< "   Cache invalidated in:"<<currThread <<'\n';
                        otherBlock->state = CacheBlockState::INVALID;//can perform a flush
                        metric.total_inval++;
                    }
                }
            }
            //Update with SHARED -> MODIFIED
            if(debug) std::cout<< "Cache updated shared to Modified in:"<<thread << '\n';
            searchBlock->state = CacheBlockState::MODIFIED;
        }
        else if(searchBlock->state == CacheBlockState::EXCLUSIVE){
            //Update with EXCLUSIVE -> MODIFIED
            if(debug) std::cout<< "Cache updated exclusive to Modified in:"<<thread << '\n';
            searchBlock->state =  CacheBlockState::MODIFIED;
        }
    }
}

void mesiSnoopyMiss(std::vector<Cache>& caches, Metrics& metric, Block * searchBlock, int requestedAddress, int thread, int readWriteBit, bool debug = false){
    //Miss -> "Search in L2", simulated by searching through all the cache
    metric.cacheMetrics[thread].cache_miss++;
    if(debug) std::cout<< "Cache miss in thread:"<<thread << '\n';
    //if read
    if(!readWriteBit){
        //issue busRd
        if(debug) std::cout<< "  Sent out bus read\n";
        metric.total_msg++;
        //If no other block, then it's exclusive 
        //Check if other cache contain the block (atomic r will mean)
        bool foundOthers = false;
        for(int currThread = 0; currThread < caches.size(); currThread++){
            if(currThread != thread){
                Block * otherBlock = caches[currThread].findBlock(requestedAddress);
                if(otherBlock){
                    if(
                        otherBlock->state == CacheBlockState::EXCLUSIVE ||
                        otherBlock->state == CacheBlockState::MODIFIED
                    )
                    {
                        //Write contents back out to memory brefore marking shared
                        //Uses 1 bus request
                        if(debug) std::cout<< "   Cache Write back to Memory:"<<currThread << " address:"<< otherBlock->address <<'\n';
                        metric.total_write_back++;
                        metric.total_msg++;
                    }
                    //Perform transition to SHARED if exists
                    if(debug) std::cout<< "   Cache state to Shared:"<<currThread <<'\n';
                    otherBlock->state = CacheBlockState::SHARED;//can perform a flush
                    //also sends acknoledgement and contents to bus
                    metric.total_ack++;
                    metric.total_msg++;
                    if(debug) std::cout<< "   Cache Acknowledged read:"<<currThread <<'\n';

                    if(!foundOthers){
                        //This is typically done through a seperate channel
                        metric.total_cache_to_cache++;
                    }
                    foundOthers = true;
                }
            }
        }
        //What is written to cache
        CacheBlockState newBlockState;
        if(foundOthers) {
            //cache replacement
            newBlockState = SHARED;
        }
        else{
            //need to fetch from memory
            if(debug) std::cout<< "   Bus Read From Memory in:"<<thread <<'\n';
            metric.total_read_mem++;
            metric.total_msg++;//technically a bus is used here
            newBlockState = EXCLUSIVE;
        }
        //Perform Cache replacement
        Block replaced = caches[thread].replaceBlock(requestedAddress, newBlockState, 0);
        if(debug){
            if(replaced.address != -1) std::cout<< "   Cache Block Replaced in:"<<thread <<'\n';
            else std::cout<< "   Cache Block Allocated in:"<<thread <<'\n';
        }
        if(replaced.state == MODIFIED){
            //perform write back
            metric.total_write_back++;
            metric.total_msg++;
            if(debug) std::cout<< "   Cache Write back to Memory:"<< thread << " address:"<< replaced.address <<'\n';
        }
        if(debug) std::cout<< "Cache block in:"<<thread << " with state:"<< newBlockState <<'\n';
    }
    //else write
    else{
        //issue BusRdX (Write invalidate)
        metric.total_msg++;
        if(debug) std::cout<< "  Sent out bus write request\n";
        //If no other block, then fetch from main memory
        bool foundOthers = false;
        //Check if other cache contain the block
        for(int currThread = 0; currThread < caches.size(); currThread++){
            if(currThread != thread){
                Block * otherBlock = caches[currThread].findBlock(requestedAddress);
                if(otherBlock){
                    foundOthers = true;
                    if(
                        //otherBlock->state == CacheBlockState::EXCLUSIVE ||
                        otherBlock->state == CacheBlockState::MODIFIED
                    )
                    {
                        //Write contents back out to memory brefore marking shared
                        //Uses 1 bus request
                        metric.total_write_back++;
                        metric.total_msg++;
                        if(debug) std::cout<< "   Cache Write back to Memory:"<<currThread << " address:"<< otherBlock->address <<'\n';
                    }
                    //also sends acknoledgement and contents to bus
                    metric.total_ack++;
                    metric.total_msg++;
                    if(debug) std::cout<< "   Cache Acknowledged read:"<<currThread <<'\n';
                    //Perform transition to INVALID if exists
                    otherBlock->state = CacheBlockState::INVALID;//can perform a flush
                    metric.total_inval++;
                    if(debug) std::cout<< "   Cache state to INVALID:"<<currThread <<'\n';
                }
            }
        }
        //What is written to cache
        CacheBlockState newBlockState = MODIFIED;
        if(!foundOthers) {
            metric.total_read_mem++;
            metric.total_msg++;//technically a bus is used here
            if(debug) std::cout<< "   Bus Read From Memory in:"<<thread <<'\n';
        }
        Block replaced = caches[thread].replaceBlock(requestedAddress, newBlockState, 0);
        if(debug){
            if(replaced.address != -1) std::cout<< "   Cache Block Replaced in:"<<thread <<'\n';
            else std::cout<< "   Cache Block Allocated in:"<<thread <<'\n';
        }
        if(replaced.state == MODIFIED){
            //perform write back
            metric.total_write_back++;
            metric.total_msg++;
            if(debug) std::cout<< "   Cache Write back to Memory:"<<thread << " address:"<< replaced.address <<'\n';
        }
        if(debug) std::cout<< "Cache block in:"<<thread << " with state:"<< newBlockState <<'\n';
    }
}

//Process request by line basis, assumption of atomicity means that processing in order should have the same number of state transitions and messages sent
void processSnoopyMemoryTraceLine(const char* line, std::vector<Cache>& caches, Metrics& metric, int block_size) {
    int time, thread, size;
    char address[9] = {0};  //Assuming memory addresses are 8 characters long, include null char
    int readWriteBit;
    sscanf(line, "%d:%d:%8[^:]:%d:%d", &time, &thread, address, &readWriteBit, &size);
    // Convert the memory address to an integer (you might want to adapt this based on your actual address format)
    int memoryAddress = std::stoi(address, 0, 16);

    //Re-work the request to the nearest cache-line boundary, described in "What Every Programmer Should Know About Memory by Ulrich Drepper, Red Hat, Inc"
    int startAddress = memoryAddress - (memoryAddress % block_size);
    int fullSize = size + memoryAddress - startAddress;

    if (thread >= caches.size()) {
        // Throw an exception if the thread index is out of bounds
        throw std::out_of_range("Thread index from file is higher than configured");
    }

    //Break down the instruction into smaller parts that work sequentially
    for(int bytesProcessed = 0; bytesProcessed < fullSize; bytesProcessed += block_size){
        int requestedAddress = startAddress + bytesProcessed;
        //MESI LOGIC in simple SNOOPY
        //std::cout << "startAddress: " << startAddress <<"\n";
        //Check L1 for hit
        Block * searchBlock = caches[thread].findBlock(requestedAddress);
        if(searchBlock){
            mesiSnoopyHit(caches, metric, searchBlock, requestedAddress,thread, readWriteBit, false);
        }
        else{
            mesiSnoopyMiss(caches, metric, searchBlock, requestedAddress,thread, readWriteBit, false);
        }
    }
}


