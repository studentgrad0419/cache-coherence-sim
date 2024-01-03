Description:
    This is a cache simulator that implements 4 kinds of snoopy coherence protocols: MSI, MESI, MOESI, and MESIF
    The bus is atomic and implements atomic transactions.
    There are also flags for debugging and running instructions in an atomicly
    This program was made under limited experience with code of other simulators, it may not be the most organized due to time.

Compile:
    In the folder that contains all the .c and .h files run this command
        g++ -std=c++11 -Wall -Wextra -o simulator.exe *.cpp -I.

        copy to benchmark folder: cp simulator ../Benchmark/

Run:
    Inputs: A memory trace file with lines in this format:
        "time: thread: mem address: access type: #bytes"
        - time is a value starting from 0, non decreasing order (more than one unique thread can show in 1 time)
        - thread is the thread number starting from 0
        - mem address is a 8 digit hex value representing an address in 32bit space
        - access type: 0 for read, 1 for write. Invalidate not supported.
        - #bytes: number of bytes to read, expect 1-8 bytes depending on datatype.
        example: 0:0:00007F55:0:7
    
    Outputs:
        file: <num_cache>_<coherency>_<inputFile_name>.txt
            contents: cache metrics from execution.
        
        if -debug is used:
            stdout: print statements of execution.

    Drag executable to where file is or vice versa
    command to run:
        ./simulator <filename> <number_caches> <cache_coherency> [options]
            [options] supported include any order of the following
                [-atomic] to process lines as if requests are instantaneous, good for manual checkup
                [-block_size <block_size>] to set size of a cache block (default 64)
                [-num_blocks <num_blocks>] to set total number of blocks in 1 cache (default 16)
                [-assoc <associativty>] to set associativty of the cache (default 1) [how many subsections is a cache split into]
                [-mem_delay <memory delay>] to set number of cycles of delay to simulate memory access (default 5)
                [-debug] to print out execution statements in std:out such as transitions, messages, and time
        
        debug example: will allow users to more easily verify manually correctness of behavior.
            ./simulator debugTrace.txt 2 MESI -debug -mem_delay 0 -num_blocks 1 -assoc 1 -atomic 

        standard example:
            ./simulator output_trace.txt 4 MESI -assoc 2
