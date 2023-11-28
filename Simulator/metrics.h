// Defines a class for storing metrics of a given run
// This simplifies the pointer being passed around for individual functions to update

// metrics.h
#ifndef METRICS_H
#define METRICS_H

#include <vector>
#include <fstream>
#include <iostream>

class CacheMetrics {    //per-cache basis
public:
    long bytes_per_set;
    long bytes_per_block;
    long bytes_total;
    long cache_miss;
    long cache_hit;
};

class Metrics {
public:
    long total_msg;
    long total_ack_all;
    long total_ack_data;
    long total_cache_to_cache; // when blocks are transferred
    long total_inval;          // total transitions into inval state
    long total_write_back;
    long total_read_mem;


    std::vector<CacheMetrics> cacheMetrics;

    Metrics(int numCaches) : cacheMetrics(numCaches) {
        cacheMetrics.resize(numCaches);
    }

    void writeToFile(const std::string &filename) const {
        std::ofstream outFile(filename);
        if (outFile.is_open()) {
            long total_cache_miss = 0;
            long total_cache_hit = 0;
            for (const auto &cacheMetric : cacheMetrics) {
                total_cache_miss += cacheMetric.cache_miss;
                total_cache_hit += cacheMetric.cache_hit;
            }
            outFile << "Total Messages: " << total_msg << "\n";
            outFile << "Total Acknowledgment: " << total_ack_all << "\n";
            outFile << "Total Acknowledgment With Data: " << total_ack_data << "\n";
            outFile << "Total Cache-to-Cache Transfers: " << total_cache_to_cache << "\n";
            outFile << "Total Invalidations: " << total_inval << "\n";
            outFile << "Total Write Backs: " << total_write_back << "\n";
            outFile << "Total Reads from Memory: " << total_read_mem << "\n";
            outFile << "Total Cache Misses: " << total_cache_miss << "\n";
            outFile << "Total Cache Hits: " << total_cache_hit << "\n";
            outFile <<"\n";

            for (size_t i = 0; i < cacheMetrics.size(); ++i) {
                outFile << "Cache " << i + 1 << " Metrics:\n";
                outFile << "  Bytes per Set: " << cacheMetrics[i].bytes_per_set << "\n";
                outFile << "  Bytes per Block: " << cacheMetrics[i].bytes_per_block << "\n";
                outFile << "  Bytes Total: " << cacheMetrics[i].bytes_total << "\n";
                outFile << "  Cache Miss: " << cacheMetrics[i].cache_miss << "\n";
                outFile << "  Cache Hit: " << cacheMetrics[i].cache_hit << "\n";

                long totalAccesses = cacheMetrics[i].cache_miss + cacheMetrics[i].cache_hit;
                double missRate = 0, hitRate = 0;
                if(totalAccesses != 0){
                    missRate = static_cast<double>(cacheMetrics[i].cache_miss) / totalAccesses;
                    hitRate = 1.0 - missRate;
                }

                outFile << "  Cache Miss Rate: " << missRate << "\n";
                outFile << "  Cache Hit Rate: " << hitRate << "\n";
                outFile <<"\n";
            }

            outFile.close();
            std::cout << "Metrics saved to file: " << filename << std::endl;
        } else {
            std::cerr << "Unable to open file: " << filename << std::endl;
        }
    }
};

#endif // METRICS_H