//Is the main program, parses arguements
//See ReadMe for instructions
#include <iostream>
#include <cstdlib>
#include <cstdint>
#include <cstring>

#include "metrics.h"
#include "runner.h"

int block_size = 64; // bytes
int num_blocks = 16; // how many blocks in a cache unit
int assoc = 1;       // how blocks are grouped in cache
char* filename;
int num_cache;
char* coherency;
int isSnoop = 1;
int mem_delay = 5;
bool debug = false;
bool runAtomicTransitions = false;

void positiveOptionsError(const char* str);
void processArgs(int argc, char** argv);

int main(int argc, char** argv) {
    // Parse and validate args
    processArgs(argc, argv);

    // Validate printing
    std::cout << "filename: " << filename << '\n';
    std::cout << "coherency: " << coherency << '\n';
    std::cout << "isSnoop: " << isSnoop << '\n';
    std::cout << "num_cache: " << num_cache << '\n';
    std::cout << "assoc: " << assoc << '\n';
    std::cout << "block_size: " << block_size << '\n';
    std::cout << "num_blocks: " << num_blocks << '\n';

    // instantiate the metrics
    Metrics allMetric = Metrics(num_cache);
    // Run the simulation (file input, cache config, metrics)
    if (strcmp(coherency, "MESI") == 0) {
        //MESI_SIM(filename, &allMetric, assoc, block_size, num_cache, num_blocks, isSnoop);
        runSim(CacheCoherency::MESI, filename, &allMetric, assoc, block_size, num_cache, num_blocks, mem_delay, debug, runAtomicTransitions);
    } else if (strcmp(coherency, "MOESI") == 0) {
        runSim(CacheCoherency::MOESI, filename, &allMetric, assoc, block_size, num_cache, num_blocks, mem_delay, debug, runAtomicTransitions);
    }
    else if (strcmp(coherency, "MESIF") == 0) {
        runSim(CacheCoherency::MESIF, filename, &allMetric, assoc, block_size, num_cache, num_blocks, mem_delay, debug, runAtomicTransitions);
    }
    else if (strcmp(coherency, "MSI") == 0) {
        runSim(CacheCoherency::MSI, filename, &allMetric, assoc, block_size, num_cache, num_blocks, mem_delay, debug, runAtomicTransitions);
    }
    else {
        std::cerr << "Unknown coherency protocol: " << coherency << std::endl;
        return 1; // Exit with an error code
    }
    // Write results (metrics to file) to file
    std::string outputFileName = std::to_string(num_cache) + "_" + std::string(coherency) + "_" + std::string(filename);
    allMetric.writeToFile(outputFileName);
    return 0;
}

void processArgs(int argc, char** argv) {
    // Basic argument check
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <filename> <number_caches> <cache_coherency> [options]\n";
        std::exit(EXIT_FAILURE);
    }

    // Required inputs
    filename = argv[1];
    num_cache = std::atoi(argv[2]);
    if (num_cache < 1) {
        positiveOptionsError("num_cache");
        std::exit(EXIT_FAILURE);
    }
    coherency = argv[3];

    // Options handling
    for (int i = 4; i < argc; ++i) {
        // Check for matches
        if (std::strcmp(argv[i], "-block_size") == 0) {
            if (i + 1 < argc) {
                block_size = std::atoi(argv[i + 1]);
                if (block_size < 1) {
                    positiveOptionsError(argv[i]);
                    std::exit(EXIT_FAILURE);
                }
                ++i;
            } else {
                positiveOptionsError(argv[i]);
                std::exit(EXIT_FAILURE);
            }
        } else if (std::strcmp(argv[i], "-num_blocks") == 0) {
            if (i + 1 < argc) {
                num_blocks = std::atoi(argv[i + 1]);
                if (num_blocks < 1) {
                    positiveOptionsError(argv[i]);
                    std::exit(EXIT_FAILURE);
                }
                ++i;
            } else {
                positiveOptionsError(argv[i]);
                std::exit(EXIT_FAILURE);
            }
        } else if (std::strcmp(argv[i], "-assoc") == 0) {
            if (i + 1 < argc) {
                assoc = std::atoi(argv[i + 1]);
                if (assoc < 1) {
                    positiveOptionsError(argv[i]);
                    std::exit(EXIT_FAILURE);
                }
                ++i;
            } else {
                positiveOptionsError(argv[i]);
                std::exit(EXIT_FAILURE);
            }
        }
        else if (std::strcmp(argv[i], "-mem_delay") == 0) {
            if (i + 1 < argc) {
                mem_delay = std::atoi(argv[i + 1]);
                if (assoc < 1) {
                    positiveOptionsError(argv[i]);
                    std::exit(EXIT_FAILURE);
                }
                ++i;
            } else {
                positiveOptionsError(argv[i]);
                std::exit(EXIT_FAILURE);
            }
        }
        else if (std::strcmp(argv[i], "-snoop") == 0) {
            isSnoop = 1;
        }
        else if (std::strcmp(argv[i], "-debug") == 0) {
            debug = 1;
        }
        else if (std::strcmp(argv[i], "-atomic") == 0) {
            runAtomicTransitions = true;
        } 
        else {
            std::cerr << "Unknown option: " << argv[i] << '\n';
            std::exit(EXIT_FAILURE);
        }
    }
}

void positiveOptionsError(const char* str) {
    std::cerr << "Usage: <filename> <number_caches> <cache_coherency> [options]\n";
    std::cerr << "Error: " << str << " option requires a positive integer value\n";
    std::exit(EXIT_FAILURE);
}