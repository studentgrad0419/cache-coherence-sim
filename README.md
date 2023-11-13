# cache-coherence-sim
A barebones cache simulator that simulates the use of different cache coherence protocols on a given input file for memory access.
The goal is to measure the number of messages sent, invalidations, and cache misses to evaluate performance of a protocol.

This is a work in progress

Simulator.c takes in 3 arguements: 
- textfile {contents will be rows of "memory access" with "r/w, address, bytes involved"}
- number of caches {reflecting the amount of l1 cache modules in the system}
- cache coherency protocol {planned to have MESI, MOESI, MESIF. More to come if time allows}

Generator.c takes up to 4 arguements:
- name of output file {it will write a txt with the given filename}
- number of accesses
- minimum access size in bytes
- maximum access size in bytes

Benchmark.c is a precompiled benchmark running against all coherency protocols using the pre-generated txt files with optional args:
- filename for list of memory access files {if none, will use filenames included in this repo}
- filename for output of results {if none, prints to console and writes to output.txt}
