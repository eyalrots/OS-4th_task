#include "../include/mmu.h"
#include <pthread.h>
#include <string.h>

void find_invalid(int *page_id, page_t *memory) {
    int i;

    for (i = 0; i< N; i++) {
        if (!memory[i].valid) {
            *page_id = i;
        }
    }
    *page_id = -1;
}

void __memory_operation(pthread_mutex_t *mem_mutex, page_t *memory, int page_id, bool *new_valid, 
                            bool *new_dirty, bool *new_reference, bool operation) {
    /*
    *   Write operation emsuring mutex.
    *   operation: READ (false) || WRITE (true).
    */
    if (!memory) {
        return;
    }
    pthread_mutex_lock(mem_mutex);
    if (operation) {
        if (new_valid)
            memory[page_id].valid = *new_valid;

        if (new_dirty)
            memory[page_id].dirty = *new_dirty;

        if (new_reference)
            memory[page_id].reference = *new_reference;
    } else {
        if (new_valid)
            *new_valid = memory[page_id].valid;

        if (new_dirty)
            *new_dirty = memory[page_id].dirty;

        if (new_reference)
            *new_reference = memory[page_id].reference;
    }
    pthread_mutex_unlock(mem_mutex);
}

void __counter_operation(pthread_mutex_t *cnt_mutex, int *num_in_mem, int* new_num, bool operation) {
    /*
    *   Write operation emsuring mutex.
    *   operation: READ (false) || WRITE (true).
    */
    if (!num_in_mem || !new_num) {
        return;
    }
    pthread_mutex_lock(cnt_mutex);
    if (operation) {
        *num_in_mem = *new_num;
    } else {
        *new_num = *num_in_mem;
    }
    pthread_mutex_unlock(cnt_mutex);
}

void main_loop(pthread_mutex_t *mem_mutex, pthread_mutex_t *cnt_mutex, pthread_mutex_t *evctr_mutex, page_t *memory, int msgid, int *num_in_mem, pthread_cond_t *mmu_cond) {
    message_t msg;
    double random;
    bool is_hit;
    bool is_full;
    bool is_empty;
    int page_id;
    struct timespec duration;
    struct timespec remaining;
    /* mutex opratiom variables */
    int  *new_cnt;
    bool *new_valid;
    bool *new_dirty;
    bool *new_reference;

    duration.tv_nsec = MEM_WR_T;
    duration.tv_sec = 0;

    while (1) {
        /* receive msg request */
        msgrcv(msgid, &msg, sizeof(msg), 1, 0);

        find_invalid(&page_id, memory);
        __counter_operation(cnt_mutex, num_in_mem, new_cnt, false);
        is_full = *new_cnt == N;
        is_empty = *new_cnt == 0;

        random = (double)rand() / RAND_MAX;
        is_hit = (random < HIT_RATE) && !is_empty;

        if (!is_hit) {
            if (is_full) {
                // SIGNAL EVICTER.
                pthread_mutex_lock(evctr_mutex);
                pthread_cond_signal(mmu_cond);
                pthread_mutex_unlock(evctr_mutex);
                // WAIT FOR SIGNAL.
                do {
                    __counter_operation(cnt_mutex, num_in_mem, new_cnt, false);
                } while (*new_cnt == N);
            }
            // REQUEST HD.
            // WAIT FOR ACK.

            /* make page valid */
            *new_valid = true;
            *new_dirty = false;
            __memory_operation(mem_mutex, memory, page_id, new_valid, new_dirty, 
                                NULL, true);

            __counter_operation(cnt_mutex, num_in_mem, new_cnt, false);
            (*new_cnt)++;
            __counter_operation(cnt_mutex, num_in_mem, new_cnt, true);
        }
        /* READ or WRITE operation */
        do {
            random = rand() % N;
            __memory_operation(mem_mutex, memory, (int)random, new_valid, NULL,
                                 NULL, false);
        } while (!new_valid);
        *new_reference = true;
        __memory_operation(mem_mutex, memory, (int)random, NULL, NULL,
                                 new_reference, true);
        if (!msg.action) {
            //SEND ACK TO PROCESS
            continue;
        }
        /* WRITE */
        nanosleep(&duration, &remaining);
        *new_dirty = true;
        __memory_operation(mem_mutex, memory, (int)random, NULL, new_dirty,
                                 NULL, true);
        // SEND ACK TO PROCESS
    }
}

void evicter_loop(pthread_mutex_t *mem_mutex, pthread_mutex_t *cnt_mutex, pthread_mutex_t *evctr_mutex, page_t *memory, int msgid, int *num_in_mem, pthread_cond_t *mmu_cond) {
    int circular_idx;
    /* mutex opratiom variables */
    int  *new_cnt;
    bool *new_valid;
    bool *new_dirty;
    bool *new_reference;

    circular_idx = 0;

    while (1) {
        pthread_mutex_lock(evctr_mutex);
        pthread_cond_wait(mmu_cond, evctr_mutex);
        pthread_mutex_unlock(evctr_mutex);

        __counter_operation(cnt_mutex, num_in_mem, new_cnt, false);

        while (*new_cnt > USED_SLOTS_TH) {
            __memory_operation(mem_mutex, memory, circular_idx, NULL, 
                                NULL, new_reference, false);
            if ((*new_reference)) {
                *new_reference = false;
                __memory_operation(mem_mutex, memory, circular_idx, NULL, 
                                NULL, new_reference, true);
                circular_idx = (circular_idx+1) % N;
                continue;
            }
            __memory_operation(mem_mutex, memory, circular_idx, NULL, 
                                new_dirty, NULL, false);
            if (!(*new_dirty)) {
                *new_dirty = false;
                __memory_operation(mem_mutex, memory, circular_idx, NULL, 
                                new_dirty, NULL, true);
                circular_idx = (circular_idx+1) % N;
                continue;
            }
            // REQUEST HD.
            // WAIT FOR ACK.

            *new_valid = false;
            *new_dirty = false;
            __memory_operation(mem_mutex, memory, circular_idx, new_valid, 
                                new_dirty, NULL, true);
            __counter_operation(cnt_mutex, num_in_mem, new_cnt, false);
            (*new_cnt)--;
            __counter_operation(cnt_mutex, num_in_mem, new_cnt, true);
        } 
    }
}
