#include "../include/mmu.h"
#include <pthread.h>
#include <stdbool.h>
#include <string.h>

int __find_invalid(page_t *memory)
{
    int i;

    for (i = 0; i < N; i++) {
        if (!memory[i].valid) {
            return i;
        }
    }
    return -1;
}

void __memory_operation(pthread_mutex_t *mem_mutex, page_t *existing_page,
                        page_t *new_page, bool operation)
{
    /*
     *   Write operation emsuring mutex.
     *   operation: READ (false) || WRITE (true).
     */
    if (operation) {
        page_write(existing_page, new_page, mem_mutex);
    } else {
        page_read(existing_page, new_page, mem_mutex);
	}
}

void __counter_operation(pthread_mutex_t *cnt_mutex, int *num_in_mem,
                         int *new_num, bool operation)
{
    /*
     *   Write operation emsuring mutex.
     *   operation: READ (false) || WRITE (true).
     */
    if (!num_in_mem || !new_num || !cnt_mutex) {
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

void main_loop(pthread_mutex_t *mem_mutex, pthread_mutex_t *cnt_mutex,
               pthread_mutex_t *evctr_mutex, page_t *memory, int msgid,
               int *num_in_mem, pthread_cond_t *mmu_cond)
{
    message_t msg;
    double random;
    bool is_hit;
    bool is_full;
    bool is_empty;
    int page_id;
    struct timespec duration;
    struct timespec remaining;
    /* mutex opratiom variables */
    int *new_cnt;
    page_t new_page;

    duration.tv_nsec = MEM_WR_T;
    duration.tv_sec = 0;

    while (1) {
        /* receive msg request */
        msgrcv(msgid, &msg, sizeof(msg), 1, 0);

        page_id = __find_invalid(memory);
        __counter_operation(cnt_mutex, num_in_mem, new_cnt, READ);
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
                    __counter_operation(cnt_mutex, num_in_mem, new_cnt, READ);
                } while (*new_cnt == N);
            }
            // REQUEST HD.
            // WAIT FOR ACK.

            /* make page valid */
            new_page.valid = true;
            new_page.dirty = false;
            new_page.reference = NULL;
            __memory_operation(mem_mutex, &memory[page_id], &new_page, WRITE);

            pthread_mutex_lock(cnt_mutex);
            (*num_in_mem)++;
            pthread_mutex_unlock(cnt_mutex);
        }
        /* READ or WRITE operation */
        do {
            random = rand() % N;
            __memory_operation(mem_mutex, &memory[(int)random], &new_page, READ);
        } while (!new_page.valid);
        new_page.valid = NULL;
        new_page.dirty = NULL;
        new_page.reference = true;
        __memory_operation(mem_mutex, &memory[(int)random], &new_page, WRITE);
        if (!msg.action) {
            // SEND ACK TO PROCESS
            continue;
        }
        /* WRITE */
        nanosleep(&duration, &remaining);
        new_page.valid = NULL;
        new_page.dirty = true;
        new_page.reference = NULL;
        __memory_operation(mem_mutex, &memory[(int)random], &new_page, WRITE);
        // SEND ACK TO PROCESS
    }
}

void evicter_loop(pthread_mutex_t *mem_mutex, pthread_mutex_t *cnt_mutex,
                  pthread_mutex_t *evctr_mutex, page_t *memory, int msgid,
                  int *num_in_mem, pthread_cond_t *mmu_cond)
{
    int circular_idx;
    /* mutex opratiom variables */
    int *new_cnt;
    page_t new_page;

    circular_idx = 0;

    while (1) {
        pthread_mutex_lock(evctr_mutex);
        pthread_cond_wait(mmu_cond, evctr_mutex);
        pthread_mutex_unlock(evctr_mutex);

        __counter_operation(cnt_mutex, num_in_mem, new_cnt, READ);

        while (*new_cnt > USED_SLOTS_TH) {
            if (second_chance(&memory[circular_idx], mem_mutex)) {
                circular_idx = (circular_idx + 1) % N;
                continue;
            }
            if (evict_clean(&memory[circular_idx], mem_mutex)) {
                circular_idx = (circular_idx + 1) % N;
                continue;
            }
            // REQUEST HD.
            // WAIT FOR ACK.

            new_page.valid = false;
            new_page.dirty = false;
            new_page.reference = NULL;
            __memory_operation(mem_mutex, &memory[circular_idx], &new_page,
                               WRITE);
            pthread_mutex_lock(cnt_mutex);
            (*num_in_mem)--;
            pthread_mutex_unlock(cnt_mutex);
        }
    }
}
