#include "../include/sim.h"
#include <pthread.h>

/* GLOBAL VARIABLES */
pthread_mutex_t mem_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  mmu_cond  = PTHREAD_COND_INITIALIZER;
page_t          memory[N];
int             num_in_mem = 0;

// void* mmu_main_thread(void* arg) {
//     main_loop(&mem_mutex, memory, 0);
// }

int main() {
    return 0;
}