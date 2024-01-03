# cache-coherence-sim
A barebones memory-trace cache simulator that simulates the use of different cache coherence protocols on a given input file for memory access.
The goal is to measure the number of messages sent, invalidations, and cache misses to evaluate performance of a protocol.

To simulate multi-core cache coherency, memory traces from multiple cores is to be expected.
This means that each "process / thread" should have its own set of memory-trace along with relative ordering.

This is a work in progress

Instructions to run as per report:  

Open zip in root directory:  
  cd Simulator/  
  g++ -std=c++11 -Wall -Wextra -g -o simulator.exe *.cpp -I.  
  cp simulator.exe ../Benchmark/  
  cd ..  
  cd Generator/  
  g++ -g -o generator.exe generator.c  
  cp generator.exe ../Benchmark/  
  cd ..  
  cd Benchmark/  
  g++ -o run_benchmarks.exe run_benchmarks.c  
  g++ -o run_sim_config.exe run_sim_config.c  
  
you can run "run_sim_config.exe" if you want to generate new tracefiles and then "run_benchmarks.exe" to generate the data  

run_sim_config will generate (and overwrite) the tracefiles as specified in the "sim_config.txt"  

run_benchmarks will run the simulation as specified in the "benchmarks.txt"  
