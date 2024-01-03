# cache-coherence-sim
A barebones memory-trace cache simulator that simulates the use of different cache coherence protocols on a given input file for memory access.
The goal is to measure the number of messages sent, invalidations, and cache misses to evaluate performance of a protocol.

To simulate multi-core cache coherency, memory traces from multiple cores is to be expected.
This means that each "process / thread" should have its own set of memory-trace along with relative ordering.

This is a work in progress

Instructions to run as per report:

Compile the Simulator from the Simulator folder:  
  g++ -std=c++11 -Wall -Wextra -static -g -o simulator *.cpp -I.  
  copy this "simulator" into the benchmark folder  

Compile the Generator from the Generator folder:  
  g++ -g -o generator .\generator.c  
  copy this "generator" into the benchmark folder  

Compile the helper functions in Benchmark folder:  
  g++ -o run_benchmarks .\run_benchmarks.c  
  g++ -o run_sim_config .\run_sim_config.c  

you can run "run_sim_config" and then "run_benchmarks"  

run_sim_config will generate the tracefiles as specified in the "sim_config.txt"  

run_benchmarks will run the simulation as specified in the "benchmarks.txt"  
