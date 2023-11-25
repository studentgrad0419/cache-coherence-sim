#ifndef MESI_H
#define MESI_H

#include "cache.h"
#include "metrics.h"

void MESI_SIM(char* filename, Metrics* metric, int associativity, int block_size, int num_cache, int num_blocks, int isSnoop);

#endif // MESI_H