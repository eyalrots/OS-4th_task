#ifndef __SIM_H__
#define __SIM_H__

#include "./mmu.h"

/* Processes unique functions */
void mmu_process(void);
void regular_process(void);
void hd_process(void);

/* Specific thread functions */
void* mmu_main_thread(void* arg);
void* mmu_evicter_thread(void* arg);
void* mmu_printer_thread(void* arg);

#endif