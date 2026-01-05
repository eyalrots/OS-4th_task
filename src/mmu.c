#include "../include/mmu.h"
#include <pthread.h>

int __find_invalid(page_t *memory)
{
    int i = 0;

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

void __hd_handler(int msgid)
{
    message_t hd_req = {.msg_type = 1, .action = false, .sender_id = getpid()};
    message_t ack = {.msg_type = 0, .action = false, .sender_id = 0};
    int msg_result = 0;

    // REQUEST HD.
    msg_result = msgsnd(msgid, &hd_req, (sizeof(hd_req) - sizeof(long)), 0);
    if (msg_result == -1 || msg_result < (sizeof(hd_req) - sizeof(long))) {
        perror(
            "Error: Message sending failed in 'main thread' on request HD.\n");
        exit(1);
    }
    // WAIT FOR ACK.
    msg_result = msgrcv(msgid, &ack, (sizeof(ack) - sizeof(long)), 2, 0);
    if (msg_result == -1 || msg_result < (sizeof(ack) - sizeof(long))) {
        perror("Error: Message receive failed in 'main thread' on HD ack.\n");
        exit(1);
    }
}

void main_loop(pthread_mutex_t *mem_mutex, pthread_mutex_t *cnt_mutex,
               pthread_mutex_t *evctr_mutex, page_t *memory, int msgid,
               int *num_in_mem, pthread_cond_t *mmu_cond)
{
    message_t msg = {.msg_type = 0, .action = false, .sender_id = 0};
    message_t ack = {.msg_type = 0, .action = false, .sender_id = 0};
    int msg_result = 0;
    double random = 0;
    bool is_hit = false;
    bool is_full = false;
    bool is_empty = true;
    int page_id = 0;
    struct timespec duration = {.tv_nsec = MEM_WR_T, .tv_sec = 0};
    struct timespec remaining = {.tv_nsec = 0, .tv_sec = 0};
    /* mutex opratiom variables */
    int *new_cnt = NULL;
    page_t new_page = {.valid = NULL, .dirty = NULL, .reference = NULL};

    while (1) {
        /* receive msg request */
        msg_result = msgrcv(msgid, &msg, (sizeof(msg) - sizeof(long)), 1, 0);
        if (msg_result == -1 || msg_result < (sizeof(msg) - sizeof(long))) {
            perror(
                "Error: Message receive failed in 'main thread' on process request.\n");
            exit(1);
        }

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
            __hd_handler(msgid);

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
            __memory_operation(mem_mutex, &memory[(int)random], &new_page,
                               READ);
        } while (!new_page.valid);
        new_page.valid = NULL;
        new_page.dirty = NULL;
        new_page.reference = true;
        __memory_operation(mem_mutex, &memory[(int)random], &new_page, WRITE);
        if (!msg.action) {
            // SEND ACK TO PROCESS
            goto proc_ack;
            continue;
        }
        /* WRITE */
        nanosleep(&duration, &remaining);
        new_page.valid = NULL;
        new_page.dirty = true;
        new_page.reference = NULL;
        __memory_operation(mem_mutex, &memory[(int)random], &new_page, WRITE);
        // SEND ACK TO PROCESS
    proc_ack:
        ack.msg_type = 2;
        ack.sender_id = getpid();
        msg_result = msgsnd(msgid, &ack, (sizeof(ack) - sizeof(long)), 0);
        if (msg_result == -1 || msg_result < (sizeof(ack) - sizeof(long))) {
            perror(
                "Error: Message sending failed in 'main thread' on process ack.\n");
            exit(1);
        }
    }
}

void evicter_loop(pthread_mutex_t *mem_mutex, pthread_mutex_t *cnt_mutex,
                  pthread_mutex_t *evctr_mutex, page_t *memory, int msgid,
                  int *num_in_mem, pthread_cond_t *mmu_cond)
{
    int circular_idx = 0;
    /* mutex opratiom variables */
    int *new_cnt = NULL;
    page_t new_page = {.valid = NULL, .dirty = NULL, .reference = NULL};

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
            __hd_handler(msgid);

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

void printer_loop(pthread_mutex_t *mem_mutex, page_t *memory)
{
    int i = 0;
    page_t local_copy_of_memory[N];
    struct timespec duration = {.tv_nsec = TIME_BETWEEN_SNAPSHOTS, .tv_sec = 0};
    struct timespec remaining = {.tv_nsec = 0, .tv_sec = 0};

    while (1) {
        nanosleep(&duration, &remaining);

        pthread_mutex_lock(mem_mutex);
        for (i = 0; i < N; i++) {
            local_copy_of_memory[i] = memory[i];
        }
        pthread_mutex_unlock(mem_mutex);

        for (i = 0; i < N; i++) {
            printf("%d|", i);
            if (local_copy_of_memory[i].valid) {
                if (local_copy_of_memory[i].dirty) {
                    printf("1\n");
                } else {
                    printf("0\n");
                }
            } else {
                printf("-\n");
            }
        }
        printf("\n\n");
    }
}
