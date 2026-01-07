#ifndef __PAGE_H__
#define __PAGE_H__

#include <stdbool.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

typedef enum action { READ, WRITE } action_t;

typedef struct page {
    bool valid;
    bool dirty;
    bool reference;
} page_t;

void page_write(page_t *existing_page, page_t *new_data,
                pthread_mutex_t *memory_mutex);

void page_read(page_t *existing_page, page_t *new_data,
               pthread_mutex_t *memory_mutex);

int page_second_chance(page_t *page, pthread_mutex_t *memory_mutex);

int page_evict_clean(page_t *page, pthread_mutex_t *memory_mutex);

#endif