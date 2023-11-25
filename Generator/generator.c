#include <stdio.h>
#include <stdlib.h>
#include <time.h>

//ToDo: based on inputs, generate memory access patterns (random, sequential, mixed/ratio)

/*  
    program info
    input: num threads, num mem access total
    output: text file per line with ":" seperated values
        line format: <time>:<thread>:<32bit mem address hex>:<access 0 for read, 1 for write>:<#of Bytes>
*/

#define MAX_THREADS 32
#define MAX_MEM_ACCESS (int)1E10

//Function that writes and generates the txt file
void generateMemoryAccessPatterns(int numThreads, int numMemAccess, const char *filename);

//MAIN
int main(int argc, char** argv){
    //variables to be set
    int numThreads;
    int numMemAccess;
    const char *filename;

    //check argc
    if(argc < 3 || argc > 4){
        printf("Usage: %s <num_threads> <num_mem_access> [filename]\n", argv[0]);
        return 1;
    }
    
    //assign var
    numThreads = atoi(argv[1]);
    numMemAccess = atoi(argv[2]);
    if(argc == 4) filename = argv[3];
    else filename = "output_trace.txt";

    //check var values
    if (numThreads < 1 || numThreads > MAX_THREADS || numMemAccess < 1 || numMemAccess > MAX_MEM_ACCESS) {
        printf("Invalid input values. Please enter valid values.\n");
        return 1;
    }

    //generate memory access patterns
    generateMemoryAccessPatterns(numThreads, numMemAccess, filename);

    printf("Memory access patterns generated successfully. Output written to %s.\n", filename);
    return 0;
}

//Generate And Write 
void generateMemoryAccessPatterns(int numThreads, int numMemAccess, const char *filename) {
    FILE *file;
    file = fopen(filename, "w");

    if (file == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    srand(time(NULL)); //seed for random number generation

    //ToDo: Parallelize loop
    for (int i = 0; i < numMemAccess; i++) {
        //generate random values for time, thread, memory address, access type, and number of bytes
        
        //Noted numerical issues with rand()%number is not statistically uniform
        int time = i;
        int thread = rand() % numThreads;       //get the thread ID
        int memAddress = rand() % (1 << 30);    //32-bit memory address
        int accessType = rand() % 2;            //0 for read, 1 for write
        int numBytes = rand() % 8 + 1;          //random number of bytes (1-8)

        //write to the file
        //%08X to pad with 0's for size of 8 characters.
        //2 ^ 32 = 16 ^ 8 so we only need 8 chars
        fprintf(file, "%d:%d:%08X:%d:%d\n", time, thread, memAddress, accessType, numBytes);
    }
    fclose(file);
}