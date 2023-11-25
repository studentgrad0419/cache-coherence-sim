// Defines a "cache" class that simulates a physical cache
// A cache is defined by block size, number of blocks, and block associativity
// The default and current replacement policy is FIFO, 
// but it can be modified to user's preference by using a different data structure

#include "cache.h"
#include "metrics.h"
#include <algorithm>

// CACHE LEVEL

// Constructor (default size is total number of blocks)
Cache::Cache(int size, int associativity) : numSets(size / associativity), sets(numSets, Set(associativity)) {}

// Find a block by address
Block* Cache::findBlock(int address) {
    int setIndex = address % numSets;
    return sets[setIndex].findBlock(address);
}

// Cache level ReplaceBlocks
Block Cache::replaceBlock(int address, CacheBlockState state, bool dirtyBit) {
    int setIndex = address % numSets;
    return sets[setIndex].replaceBlock(address, state, dirtyBit);
}

// Find a replacement block in the cache for the given address
Block* Cache::findReplacementBlock(int address) {
    int setIndex = address % numSets;
    return sets[setIndex].findReplacementBlock();
}

// SETS LEVEL

// Constructor for the set
Set::Set(int associativity) : associativity(associativity) {
    blocks.resize(associativity);
}

//Find block within set
Block* Set::findBlock(int address) {
    auto it = std::find_if(blocks.begin(), blocks.end(),
        [&](const Block& b) { return b.address == address;});
    return (it != blocks.end()) ? &(*it) : nullptr;
}


// Replace a block in FIFO order within the set
Block Set::replaceBlock(int address, CacheBlockState state, bool dirtyBit) {
    Block oldBlock;
    if (!blocks.empty()){
        oldBlock = blocks.front();
        blocks.pop_front(); //Remove the oldest block
    }
    Block newBlock = Block();
    newBlock.state = state;
    newBlock.dirtyBit = dirtyBit;
    newBlock.address = address;
    blocks.push_back(newBlock); //Add the new block to the back
    return oldBlock;
}

// Find a replacement block in the set for the given address
Block* Set::findReplacementBlock() {
    // You can implement your replacement policy here
    Block* oldBlock;
    if (!blocks.empty()){
        oldBlock = &blocks.front();
    }
    return oldBlock;
}

//DIRECTORY LEVEL (for directory-based alg)

Directory::Directory(int numCaches){
    this->numCaches = numCaches;
    std::unordered_map<int, std::vector<std::tuple<bool, CacheBlockState>>> directoryEntries;
}

//Implement Directory methods
void Directory::updateDirectoryEntry(int address, int cacheIndex, bool dirtyBit, CacheBlockState state) {
    // Check if the address is already in the directory
    auto it = directoryEntries.find(address);
    if (it != directoryEntries.end()) {
        // Update existing entry
        it->second[cacheIndex] = std::make_tuple(dirtyBit, state);
    } else {
        // Create a new entry with 0-initialized values for other caches
        //numCaches is from the Directory class constructor/ private variable
        std::vector<std::tuple<bool, CacheBlockState>> newEntry(numCaches, std::make_tuple(false, CacheBlockState::INVALID));
        newEntry[cacheIndex] = std::make_tuple(dirtyBit, state);
        directoryEntries[address] = newEntry;
    }
}

void Directory::removeDirectoryEntry(int address) {
    directoryEntries.erase(address);
}

std::tuple<bool, CacheBlockState> Directory::getCacheLineInfo(int address, int cacheIndex) {
    auto it = directoryEntries.find(address);
    if (it != directoryEntries.end()) {
        return it->second[cacheIndex];
    } else {
        // Return a default value if the address is not found
        return std::make_tuple(false, CacheBlockState::INVALID);
    }
}

