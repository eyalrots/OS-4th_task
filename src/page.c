#include "../include/page.h"

void page_write(page_t *existing_page, page_t *new_data,
                pthread_mutex_t *memory_mutex)
{
    if (!existing_page || !new_data) {
        return;
    }
    pthread_mutex_lock(memory_mutex);
    *existing_page = *new_data;
    pthread_mutex_unlock(memory_mutex);
}

void page_read(page_t *existing_page, page_t *new_data,
               pthread_mutex_t *memory_mutex)
{
    if (!existing_page || !new_data) {
        return;
    }
    pthread_mutex_lock(memory_mutex);
    *new_data = *existing_page;
    pthread_mutex_unlock(memory_mutex);
}

int page_second_chance(page_t *page, pthread_mutex_t *memory_mutex)
{
    page_t new_data;
    memset(&new_data, 0, sizeof(new_data));

    if (!page || !memory_mutex) {
        return 0;
    }

    page_read(page, &new_data, memory_mutex);

    if (new_data.reference) {
        new_data.valid = page->valid;
        new_data.dirty = page->dirty;
        new_data.reference = false;
        page_write(page, &new_data, memory_mutex);
        return 1;
    }

    return 0;
}

int page_evict_clean(page_t *page, pthread_mutex_t *memory_mutex)
{
    page_t new_data;
    memset(&new_data, 0, sizeof(new_data));

    if (!page || !memory_mutex) {
        return 0;
    }

    page_read(page, &new_data, memory_mutex);

    if (!new_data.dirty) {
        new_data.valid = false;
        new_data.dirty = page->dirty;
        new_data.reference = page->reference;
        page_write(page, &new_data, memory_mutex);
        return 1;
    }

    return 0;
}
