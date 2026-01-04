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
#include "../include/config.h"

typedef struct page {
    bool valid;
    bool dirty;
    bool reference;
}page_t;

typedef struct msg_buffer {
    long msg_type;  /* Request (1) || Ack (2) */
    int  sender_id; 
    int  action;    /* READ (0) || WRITE (1) */
} message_t;

void main_loop(pthread_mutex_t *mem_mutex, pthread_mutex_t *cnt_mutex, pthread_mutex_t *evctr_mutex, 
                page_t *memory, int msgid, int *num_in_mem, pthread_cond_t *mmu_cond);
void evicter_loop(pthread_mutex_t *mem_mutex, pthread_mutex_t *cnt_mutex, pthread_mutex_t *evctr_mutex, 
                    page_t *memory, int msgig, int *num_in_mem, pthread_cond_t *mmu_cond);
void printer_loop(pthread_mutex_t *mem_mutex, page_t *memory);

#endif