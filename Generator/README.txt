Description:

    A random memory trace generator that generates lines 
    in a txt file that follows the following format: 
        "time: thread: mem address: access type: #bytes"
        - time is a value starting from 0, non decreasing order (more than one unique thread can show in 1 time)
        - thread is the thread number starting from 0
        - mem address is a 8 digit hex value representing an address in 32bit space
        - access type: 0 for read, 1 for write. Invalidate not supported.
        - #bytes: number of bytes to read, expect 1-8 bytes depending on datatype.
        example: 0:0:00007F55:0:7
    
    This random trace generator is inspired by the access patterns mentioned in 
        "Comparison of hardware and software cache coherence schemes" (S. Adve, et al) 1991.
        
    It features the 3 types of memory access patterns mentioned in the paper as well as a pure random option.
    It also features a scramble flag that uses a random number to divide the parts into smaller sections if desired.
    
    While the input is ratio_mr, ratio_rw, and i_mg, other parts for the parameterization of data behavior is assumed
        for example: 
        - n_mr / n_rw = number of threads that read before write is assumed to be all threads in the system.
        - I_mr/ I_rw  = number of accesses before invalidation is enforced, if user defines it will be 1 to reflect behavior of figure 5a


COMPILE:

in directory with generator.c
    g++ -g -o generator.exe generator.c

additionally move to benchmark folder
    cp generator.exe ../Benchmark/


USAGE: (no guarantees if not used properly)

    input format:
        ./generator.exe <num mem access> <thread count> <num m-r> <num many r-w> <num migration> <ratio_mr> <ratio_rw> <i_mg> [scramble flag] [seed] [filename]
        - <> means required
        - <num mem access> (int) is the number of memory access to be generated**. <max 1,000,000,000>
        - <thread count>   (int) is the number of threads to generate, 0-indexed. <max 32>
        - <num many m-r>   (int) is the relative amount of "many reads" memory patterns with little expected cache invalidation
        - <num many r-w>   (int) is the relative amount of "read-write" memory patterns with high expected cache invalidation
        - <num migration>  (int) is the relative amount of "migratory data" memory patterns for things like locks where a single thread may access multiple times before transfering.
        - <ratio_mr>       (double) is the parameter based on the paper, the higher the number the more expected read request
        - <ratio_rw>       (double) is the parameter based on the paper, similar to ratio_mr but needs to be a lower value to work as expected
        - <i_mg>           (int) is the number of instructions a cpu is expected to have sole access to the block before giving it up
        - [scramble flag]  (0 / 1) is to enable random splicing of sections so that they will appear in order and on repeat until all memaccess is used up.
        - [seed]           (int) is a seed to create a reproducible result.
        - [filename]       (string) if desired. default is formated "trace_<num thread>_<num m-r>_<num_r-w>_<num_migration>.txt"
        
        **note due to potential looping and division, the final file may not have the exact number of memory access.
        ***note if user defines ratio_mr and/or ratio_rw then expect I_mr/ I_rw to equal 1 for write probability 
            this is to match closer to the curves shown in the paper.

    Option 1: to use the purely random option
        ./generator.exe <number of memory access> <thread count> <0> <0> <0>

        example:
        ./generator.exe 25 4 0 0 0
        ./generator.exe 25 4 0 0 0 0 0 0
    

    Option 2: to use ratios only (this will use randomized parameters similar to the paper) 
        ./generator.exe <number of memory access> <thread count> <3> <2> <1>

        example:
        ./generator.exe 25 4 3 2 1
        ./generator.exe 25 4 3 2 1 0 0 0
    

    Option 3: to use custom values for ratios (will have interesting results, but may not be as useful)
        ./generator.exe <number of memory access> <thread count> <3> <2> <1> <2.1> <6.0> <6>

        example:
        ./generator.exe 25 4 3 2 1 2.1 6.0 6 



    


