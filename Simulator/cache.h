// Defines a "cache" class that simulates a physical cache
// A cache is defined by block size, number of blocks, and block associativity
// The default and current replacement policy is FIFO, 
// but it can be modified to user's preference by using a different data structure
// Directory for all caches is also defined, but is not used in simulation due to time.
// Enum for all used states across tested coherency is listed here.
#ifndef CACHE_H
#define CACHE_H

#include <vector>
#include <tuple>
#include <deque>
#include <unordered_map>
#include <string>

// Enum for cache block state
// This is shared in one place for convenience of programming 
enum CacheBlockState {
    INVALID,
    VALID,
    DIRTY,
    SHARED,
    EXCLUSIVE,
    MODIFIED,
    OWNED,
    FORWARD,
};

//string method for enum
std::string cacheBlockStateToString(CacheBlockState state);

// Block class representing a cache block
class Block{
public:
    CacheBlockState state;
    int address;
    bool dirtyBit;
    Block() : state(INVALID), address(-1), dirtyBit(false) {}
};

// SET Class, groups up blocks, split by address % num_sets
class Set {
public:
    Set(int associativity);
    Block* findBlock(int address);
    Block replaceBlock(int address, CacheBlockState state, bool dirtyBit);
    //Should generally be FIFO queue head if using fifo
    Block* findReplacementBlock();

//private:
    int associativity;
    std::deque<Block> blocks;
};


// Cache class consists of sets of blocks
class Cache {
public:
    Cache(int size, int associativity);

    Block* findBlock(int address);
    Block replaceBlock(int address, CacheBlockState state, bool dirtyBit);
    Block* findReplacementBlock(int address);//Should generally be FIFO queue head of relevant SET

//private:
    int numSets;
    std::vector<Set> sets;
};

class Directory {
public:
    //Define shape of entries
    Directory(int numCaches);
    
    // Update directory entry for a cache line
    void updateDirectoryEntry(int address, int cacheIndex, bool dirtyBit, CacheBlockState state);

    // Remove directory entry for a cache line
    void removeDirectoryEntry(int address);

    // Get the state of a cache line from the directory
    std::tuple<bool, CacheBlockState> getCacheLineInfo(int address, int cacheIndex);

//private:
    // Directory entries for each cache line
    std::unordered_map<int, std::vector<std::tuple<bool, CacheBlockState>>> directoryEntries;
    int numCaches;
};

#endif // CACHE_H