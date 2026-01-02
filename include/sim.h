#ifndef SIM_H
#define SIM_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <time.h>

#include "CONFIG.H"

typedef struct { 
    long mytype;
    int sender_id;
    int is_write; 

}msg_buffer;

#endif
