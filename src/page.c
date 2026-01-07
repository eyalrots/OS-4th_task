#include "../include/page.h"

void page_write(page_t *existing_page, page_t *new_data,
                pthread_mutex_t *memory_mutex)
{
    if (!existing_page || !new_data) {
        return;
    }
    pthread_mutex_lock(memory_mutex);
    if (new_data->valid) {
        existing_page->valid = new_data->valid;
    }
    if (new_data->dirty) {
        existing_page->dirty = new_data->dirty;
    }
    if (new_data->reference) {
        existing_page->reference = new_data->reference;
    }
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
    page_t *new_data;

    if (!page || !memory_mutex) {
        return 0;
    }

    page_read(page, new_data, memory_mutex);

    if (new_data->reference) {
        new_data->valid = NULL;
        new_data->dirty = NULL;
        new_data->reference = false;
        page_write(page, new_data, memory_mutex);
        return 1;
    }

    return 0;
}
int page_evict_clean(page_t *page, pthread_mutex_t *memory_mutex)
{
    page_t *new_data;

    if (!page || !memory_mutex) {
        return 0;
    }

    page_read(page, new_data, memory_mutex);

    if (!new_data->dirty) {
        new_data->valid = false;
        new_data->dirty = NULL;
        new_data->reference = NULL;
        page_write(page, new_data, memory_mutex);
        return 1;
    }

    return 0;
}
