#include "../include/mmu.h"
#include <unistd.h>

static int mmu_find_invalid(page_t *memory)
{
    int i = 0;

    for (i = 0; i < N; i++) {
        if (!memory[i].valid) {
            return i;
        }
    }
    return -1;
}

static void mmu_memory_operation(pthread_mutex_t *mem_mutex,
                                 page_t *existing_page, page_t *new_page,
                                 bool operation)
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

static void mmu_counter_operation(pthread_mutex_t *cnt_mutex, int *num_in_mem,
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

static void mmu_hd_handler(int msgid, action_t action)
{
    message_t hd_req;
    message_t ack;
    int msg_result = 0;
    type_t ack_type = 0;

    /* Initialization */
    memset(&ack, 0, sizeof(ack));
    memset(&hd_req, 0, sizeof(hd_req));

    hd_req.action = action;
    hd_req.msg_type = HD_REQUEST;
    hd_req.sender_id = getpid();

    // REQUEST HD.
    msg_result = msgsnd(msgid, &hd_req, (sizeof(hd_req) - sizeof(long)), 0);
    if (msg_result == -1 || msg_result < (sizeof(hd_req) - sizeof(long))) {
		perror(
			"Error: Message sending failed in 'main thread' on request HD.\n");
		}
	printf("Sending message to HD from %d with type %ld.\n", getpid(), (long)hd_req.msg_type);
    // WAIT FOR ACK.
    ack_type = action + HD_ACK;
    msg_result = msgrcv(msgid, &ack, (sizeof(ack) - sizeof(long)), (long)ack_type, 0);
    if (msg_result == -1 || msg_result < (sizeof(ack) - sizeof(long))) {
        perror("Error: Message receive failed in 'main thread' on HD ack.\n");
    }
    printf("Received ack from HD in %d with type %ld.\n", getpid(), (long)ack_type);
}

void mmu_main_loop(pthread_mutex_t *mem_mutex, pthread_mutex_t *cnt_mutex,
                   pthread_mutex_t *evctr_mutex, page_t *memory, int msgid,
                   int *num_in_mem, pthread_cond_t *mmu_cond)
{
    message_t msg;
    message_t ack;
    int msg_result = 0;
    double random = 0;
    bool is_hit = false;
    bool is_full = false;
    bool is_empty = true;
    int page_id = 0;
    struct timespec duration = {.tv_nsec = MEM_WR_T, .tv_sec = 0};
    struct timespec remaining = {.tv_nsec = 0, .tv_sec = 0};
    /* mutex opratiom variables */
    int new_cnt = 0;
    page_t new_page = {.valid = NULL, .dirty = NULL, .reference = NULL};

    /* Iitialization */
    memset(&msg, 0, sizeof(msg));
    memset(&ack, 0, sizeof(ack));

    while (1) {
        /* receive msg request */
        msg_result = msgrcv(msgid, &msg, (sizeof(msg) - sizeof(long)),
                            (long)MMU_REQUEST, 0);
        if (msg_result == -1 || msg_result < (sizeof(msg) - sizeof(long))) {
            perror(
                "Error: Message receive failed in 'main thread' on process request.\n");
        }
        page_id = mmu_find_invalid(memory);
        mmu_counter_operation(cnt_mutex, num_in_mem, &new_cnt, (bool)READ);
        is_full = new_cnt == N;
        is_empty = new_cnt == 0;

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
                    mmu_counter_operation(cnt_mutex, num_in_mem, &new_cnt,
                                          (bool)READ);
                } while (new_cnt == N);
            }
            // REQUEST HD.
            mmu_hd_handler(msgid, READ);

            /* make page valid */
            new_page.valid = true;
            new_page.dirty = false;
            new_page.reference = NULL;
            mmu_memory_operation(mem_mutex, &memory[page_id], &new_page,
                                 (bool)WRITE);

            pthread_mutex_lock(cnt_mutex);
            (*num_in_mem)++;
            pthread_mutex_unlock(cnt_mutex);
        }
        /* READ or WRITE operation */
        do {
            random = rand() % N;
            mmu_memory_operation(mem_mutex, &memory[(int)random], &new_page,
                                 (bool)READ);
        } while (!new_page.valid);
        new_page.valid = NULL;
        new_page.dirty = NULL;
        new_page.reference = true;
        mmu_memory_operation(mem_mutex, &memory[(int)random], &new_page,
                             (bool)WRITE);
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
        mmu_memory_operation(mem_mutex, &memory[(int)random], &new_page,
                             (bool)WRITE);
        // SEND ACK TO PROCESS
    proc_ack:
        ack.msg_type = msg.sender_id + (long)MMU_ACK;
        ack.sender_id = getpid();
        msg_result = msgsnd(msgid, &ack, (sizeof(ack) - sizeof(long)), 0);
        if (msg_result == -1 || msg_result < (sizeof(ack) - sizeof(long))) {
            perror(
                "Error: Message sending failed in 'main thread' on process ack.\n");
        }
    }
}

void mmu_evicter_loop(pthread_mutex_t *mem_mutex, pthread_mutex_t *cnt_mutex,
                      pthread_mutex_t *evctr_mutex, page_t *memory, int msgid,
                      int *num_in_mem, pthread_cond_t *mmu_cond)
{
    int circular_idx = 0;
    /* mutex opratiom variables */
    int new_cnt = 0;
    page_t new_page = {.valid = NULL, .dirty = NULL, .reference = NULL};

    while (1) {
        pthread_mutex_lock(evctr_mutex);
        pthread_cond_wait(mmu_cond, evctr_mutex);
        pthread_mutex_unlock(evctr_mutex);
        mmu_counter_operation(cnt_mutex, num_in_mem, &new_cnt, (bool)READ);

        while (new_cnt > USED_SLOTS_TH) {
            if (page_second_chance(&memory[circular_idx], mem_mutex)) {
                circular_idx = (circular_idx + 1) % N;
                continue;
            }
            if (page_evict_clean(&memory[circular_idx], mem_mutex)) {
                circular_idx = (circular_idx + 1) % N;
                continue;
            }
            // REQUEST HD.
            // WAIT FOR ACK.
            mmu_hd_handler(msgid, WRITE);

            new_page.valid = false;
            new_page.dirty = false;
            new_page.reference = NULL;
            mmu_memory_operation(mem_mutex, &memory[circular_idx], &new_page,
                                 (bool)WRITE);
            pthread_mutex_lock(cnt_mutex);
            (*num_in_mem)--;
            pthread_mutex_unlock(cnt_mutex);
        }
    }
}

void mmu_printer_loop(pthread_mutex_t *mem_mutex, page_t *memory)
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
