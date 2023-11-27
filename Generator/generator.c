#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Classes of memory access types and formulas derived from 
//"Comparison of hardware and software cache coherence schemes" (S. Adve, et al) 1991. 
// User provides ratio_mr, ratio_rw, and i_mg

/*  
    program info
    input: num threads, lines of mem_access, MostlyReadRatio, FrequentlyWrittenRatio, MigratoryRatio, 
    output: text file per line with ":" seperated values
        line format: <time>:<thread>:<32bit mem address hex>:<access 0 for read, 1 for write>:<#of Bytes>
*/

#define MAX_THREADS 32
#define BLOCK_SIZE 64
#define MAX_MEM_ACCESS (int)1E10

//Function that writes and generates the txt file
void generateMemoryAccessPatterns(int numThreads, int numMemAccess, const char *filename, int *thread_list);
void generateMostlyReadSection(int numThreads, int numMemAccess, const char *filename, double ratio_mr, int num_sections, int *thread_list);
void generateFrequentRWSection(int numThreads, int numMemAccess, const char *filename, double ratio_rw,  int num_sections, int *thread_list);
void generateMigratorySection(int numThreads, int numMemAccess, const char *filename, int num_singe_access, int *thread_list);
int rangeWeightedRandInt(int range);
void shuffleThreads(int *thread_list, int size);

int global_time = 0;

//MAIN
int main(int argc, char **argv) {
    if (argc < 6 || argc > 11) {
        printf("Usage: %s <total_mem_access> <num threads> <MostlyRead#> <FreqRW#> <MigrS#> <ratio_mr> <ratio_rw> <i_mg> [scramble flag] [seed] [filename]\n", argv[0]);
        return 1;
    }

    // Default seed value
    int seed = time(NULL);
    // Check if a seed is provided as a command-line argument
    if (argc == 12) {
        seed = atoi(argv[11]);
    }

    // Set the seed for the random number generator
    srand(seed);

    int totalMemAccess = atoi(argv[1]);
    int numThreads = atoi(argv[2]);
    int mostlyReadRatio = atoi(argv[3]);
    int freqRWRatio = atoi(argv[4]);
    int migrSRatio = atoi(argv[5]);
    int scrambleFlag = 0; // Default is no scramble
    double ratio_mr = 0.0; // Default value defined later 
    double ratio_rw = 0.0; //Default value defined later
    int i_mg = 0; // Allow random value later
    const char *filename;

    if (argc >= 8) {
        ratio_mr = atof(argv[6]);
        ratio_rw = atof(argv[7]);
        i_mg = atof(argv[8]);
    }

    if (argc >= 10) {
        scrambleFlag = atoi(argv[9]);
    }
    //NEED 
    if (argc == 11 || argc == 12) {
        filename = argv[10];
    } 
    else {
        // Default filename format: trace_#threads_MR#_FRW#_Migr#.txt
        char defaultFilename[50];
        sprintf(defaultFilename, "trace_%d_%d_%d_%d.txt", numThreads, mostlyReadRatio, freqRWRatio, migrSRatio);
        filename = defaultFilename;
    }

    //initialize file
    FILE *file;
    file = fopen(filename, "w");
    if (file == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }
    fclose(file);

    //Initialize a pool of threads to randomize and pick x from pool to send at one time slot
    int *threads_order = (int *) malloc(numThreads * sizeof(int));
    if (threads_order == NULL) {
        perror("Memory allocation error");
        return 1;
    }
    for (int i = 0; i < numThreads; i++) {
        threads_order[i] = i;
    }

    // Calculate the number of memory accesses for each pattern type
    int totalSections = mostlyReadRatio + freqRWRatio + migrSRatio;
    int mostlyReadSections = 0;
    int freqRWSections = 0;
    int migrSections = 0;

    if(totalSections != 0){
        mostlyReadSections = (mostlyReadRatio * totalMemAccess) / totalSections;
        freqRWSections = (freqRWRatio * totalMemAccess) / totalSections;
        migrSections = (migrSRatio * totalMemAccess) / totalSections;
    }
    
    int isAllRandom = (mostlyReadSections < 1 && freqRWSections < 1 && migrSections < 1);
    //If all 3 are 0 or less, it's full random in incrementing time
    if(isAllRandom){
        generateMemoryAccessPatterns(numThreads, totalMemAccess, filename, threads_order);
    }
    else{
        // "Scramble" into sections (like round robin them)
        int roundPeriod = 1;
        if(scrambleFlag){
            //random split (recommend for higher random and larger memsizes)
            roundPeriod = rand() % (totalMemAccess / 8) + 1;
        }
        for(int round = 0; round < roundPeriod; round++){

            //Mostly Read + random param based on paper
            int numMemAccess = mostlyReadSections / roundPeriod;
            int num_sections = rand() % 4 + 1; // Num sections = rand(1:4)
            if(ratio_mr < 1E-8){
                double fractionWrite = ((double)rand() / RAND_MAX) * 0.1; // Fraction Write <= 0.1
                int mean_thread_per_section = rand() % numThreads + 1; // Mean threads per section <= numThreads
                double rand_ratio_mr = (1.0 - fractionWrite) / (fractionWrite * (double)numThreads * ((double)numMemAccess / (double)num_sections));
                generateMostlyReadSection(numThreads, numMemAccess, filename, rand_ratio_mr, num_sections, threads_order);
            }
            else generateMostlyReadSection(numThreads, numMemAccess, filename, ratio_mr, num_sections, threads_order);
            
            //Frequent RW section + random param
            numMemAccess = freqRWSections / roundPeriod;
            num_sections = rand() % 4 + 1;
            if(ratio_rw < 1E-8){
                double fractionWrite = ((double)rand() / RAND_MAX) * 0.4 + 0.1; // Fraction Write >= 0.1, < 0.5
                int mean_thread_per_section = rand() % numThreads + 1; // Mean threads per section <= numThreads
                double rand_ratio_rw = (1.0 - fractionWrite) / (fractionWrite * (double)numThreads * ((double)numMemAccess / (double)num_sections));
                generateFrequentRWSection(numThreads, numMemAccess, filename, rand_ratio_rw, num_sections, threads_order);
            }
            else generateFrequentRWSection(numThreads, numMemAccess, filename, ratio_rw, num_sections, threads_order);

            numMemAccess = migrSections / roundPeriod;
            if(i_mg == 0) {
                int num_singe_access = rand() % 7 + 2; // Num sections = rand(2:8)
                generateMigratorySection(numThreads, numMemAccess, filename, num_singe_access, threads_order);
            }
            generateMigratorySection(numThreads, numMemAccess, filename, i_mg, threads_order);
        }
    }
    printf("Memory access patterns generated successfully. Output written to %s.\n", filename);
    return 0;
}

//Helper for weighted randomness
int rangeWeightedRandInt(int range) {
    int totalWeight = 0;
    for (int i = 0; i < range; ++i) {
        totalWeight += i;
    }

    // Generate a random number in the range 0 to totalWeight - 1
    int randomValue = rand() % totalWeight;
    // Determine the selected value based on the weights
    int selectedValue = 0;
    int cumulativeWeight = 0;
    for (int i = 0; i < range; ++i) {
        //Each step is progressively larger
        cumulativeWeight += i;
        //If inside this step
        if (randomValue < cumulativeWeight) {
            selectedValue = i;
            break;
        }
    }
    return selectedValue;
}

//PURE RANDOM GENERATOR
void generateMemoryAccessPatterns(int numThreads, int numMemAccess, const char *filename, int *thread_list) {
    FILE *file;
    file = fopen(filename, "w");

    if (file == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    int totalAccess = 0;
    while(totalAccess < numMemAccess) {
        //At given time, how many thread requests
        int threads_access = numThreads - rangeWeightedRandInt(numThreads);
        int randomBlock = rand() % (1 << 30);
        shuffleThreads(thread_list, numThreads);
        for(int i = 0; i< threads_access; ++i){
            int thread = thread_list[i];
            int accessType = rand() % 2; 
            int numBytes = rand() % 8 + 1;
            fprintf(file, "%d:%d:%08X:%d:%d\n", global_time, thread, randomBlock, accessType, numBytes);
            totalAccess++;
            if(totalAccess >= numMemAccess) break;
        }
        global_time++;
    }

    fclose(file);
}

//To simulate a similar access pattern to the paper
void generateMostlyReadSection(int numThreads, int numMemAccess, const char *filename, double ratio_mr, int num_sections, int *thread_list){
    if (numMemAccess == 0) return;

    //File Setup
    FILE *file;
    file = fopen(filename, "a");//Append to file
    if (file == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    //Accesses per section before all must invalidate (in my case, switch block mem)
    int access_per_section = numMemAccess / num_sections;

    // My implementation (n_mr -> all cores access to access before write, l_mr -> how many references before forced ->I)
    // ratio_mr = P(read) / [ P(write) * num_cores * access_per_section]
    // solve for P(Write) =>  P(read) = P(Write) * ratio_mr * num_cores * access_per_section
    double pWrite = 1.0 / (ratio_mr * numThreads * access_per_section);
    
    //Generate until number of access is desired and enforce statistics
    int local_time = global_time;
    int totalAccess = 0;
    while(totalAccess < numMemAccess){
        //Reset block to simulate system inv instr. (now cores all need a fresh copy)
        int shared_block = rand() % (1 << 30);
        int numRead = 0;
        int localAccessAmt = 0;
        while(localAccessAmt < access_per_section){
            //figure out how many threads r/w at once
            int threads_access = numThreads - rangeWeightedRandInt(numThreads);
            shuffleThreads(thread_list, numThreads);
            for(int i = 0; i < threads_access; i++){
                int thread = thread_list[i];
                int numBytes = rand() % 8 + 1;
                int rwbit = 1;

                //Random Read based on fraction write
                if((rand() / (double)RAND_MAX) > pWrite) {rwbit = 0; numRead++; }
                //Force write bit if too many reads (with some randomness +-2)
                if(numRead >= numThreads + (rand() % 2)) { numRead = 0; rwbit = 1;}

                fprintf(file, "%d:%d:%08X:%d:%d\n", local_time, thread, shared_block, rwbit, numBytes);
                totalAccess++;
                localAccessAmt++;
                if(localAccessAmt >= access_per_section || totalAccess >= numMemAccess) break;
            }
            local_time++;
            if(totalAccess >= numMemAccess) break;
        }
    }

    fclose(file);
    global_time = local_time;
}


//Similar to generate MR
void generateFrequentRWSection(int numThreads, int numMemAccess, const char *filename, double ratio_rw, int num_sections, int *thread_list){
    if(numMemAccess == 0) return;

    //File Setup
    FILE *file;
    file = fopen(filename, "a");//Append to file
    if (file == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    //Accesses per section before all must invalidate (in my case, switch block mem)
    int access_per_section = numMemAccess / num_sections;

    // My implementation (n_mr -> all cores access to access before write, l_mr -> how many references before forced ->I)
    // ratio_rw = P(read) / [ P(write) * num_cores * access_per_section]
    // solve for P(Write) =>  P(read) = P(Write) * ratio_rw * num_cores * access_per_section
    double pWrite = 1.0 / (ratio_rw * numThreads * access_per_section);
    
    //Generate until number of access is desired and enforce statistics
    int local_time = global_time;
    int totalAccess = 0;
    while(totalAccess < numMemAccess){
        //Reset block to simulate system inv instr. (now cores all need a fresh copy)
        int shared_block = rand() % (1 << 30) ;
        int numRead = 0;
        int localAccessAmt = 0;
        while(localAccessAmt < access_per_section){
            //figure out how many threads r/w at once
            int threads_access = numThreads - rangeWeightedRandInt(numThreads);
            shuffleThreads(thread_list, numThreads);
            for(int i = 0; i < threads_access; i++){
                int thread = thread_list[i];
                int numBytes = rand() % 8 + 1;
                int rwbit = 1;

                //Random Read based on fraction write
                if((rand() / (double)RAND_MAX) > pWrite) {rwbit = 0; numRead++; }
                //Force write bit if too many reads (with some randomness +-2)
                if(numRead >= numThreads + (rand() % 2)) { numRead = 0; rwbit = 1;}

                fprintf(file, "%d:%d:%08X:%d:%d\n", local_time, thread, shared_block, rwbit, numBytes);
                totalAccess++;
                localAccessAmt++;
                if(localAccessAmt >= access_per_section || totalAccess >= numMemAccess) break;
            }
            local_time++;
            if(totalAccess >= numMemAccess) break;
        }
    }
    fclose(file);
    global_time = local_time;
}


void generateMigratorySection(int numThreads, int numMemAccess, const char *filename, int num_singe_access, int *thread_list){
    if (numMemAccess == 0) return;

    //File Setup
    FILE *file;
    file = fopen(filename, "a");//Append to file
    if (file == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    int local_time = global_time;
    int totalAccess = 0;
    while(totalAccess < numMemAccess) {
        //Read n times by 1 random processor
        int thread = rand() % numThreads;
        int shared_block = rand() % (1 << 30);
        
        for(int j = 0; j< num_singe_access; ++j){
            int numBytes = rand() % 8 + 1;
            fprintf(file, "%d:%d:%08X:%d:%d\n", local_time, thread, shared_block, 0, numBytes);
            totalAccess++;
            local_time++;
            if(totalAccess >= numMemAccess) break;
        }
        if(totalAccess >= numMemAccess) break;
        
        //write once by a random thread
        thread = rand() % numThreads;
        int numBytes = rand() % 8 + 1;
        fprintf(file, "%d:%d:%08X:%d:%d\n", local_time, thread, shared_block, 1, numBytes);
        local_time++;
        totalAccess++;
    }
    fclose(file);
    global_time = local_time;
}

//Shuffle pool of threads
void shuffleThreads(int *thread_list, int size) {
    //Standard random shuffle alg.
    for (int i = size - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        // Swap thread_list[i] and thread_list[j]
        int temp = thread_list[i];
        thread_list[i] = thread_list[j];
        thread_list[j] = temp;
    }
}