#ifndef __MMU_H__
#define __MMU_H__

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include "../include/config.h"
#include "../include/page.h"

typedef enum type {
    MMU_REQUEST = 1,
    MMU_ACK,
    HD_REQUEST,
    HD_ACK
} type_t;

typedef struct __attribute__((packed)) msg_buffer {
    long msg_type;
    int sender_id;
    int action; /* READ (0) || WRITE (1) */
} message_t;

void mmu_main_loop(pthread_mutex_t *mem_mutex, pthread_mutex_t *cnt_mutex,
               pthread_mutex_t *evctr_mutex, page_t *memory, int msgid,
               int *num_in_mem, pthread_cond_t *mmu_cond);

void mmu_evicter_loop(pthread_mutex_t *mem_mutex, pthread_mutex_t *cnt_mutex,
                  pthread_mutex_t *evctr_mutex, page_t *memory, int msgig,
                  int *num_in_mem, pthread_cond_t *mmu_cond);

void mmu_printer_loop(pthread_mutex_t *mem_mutex, page_t *memory);

#endif