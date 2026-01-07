#include "../include/sim.h"

/* GLOBAL VARIABLES */
pthread_mutex_t mem_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cnt_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cond_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t mmu_cond = PTHREAD_COND_INITIALIZER;
page_t memory[N];
int num_in_mem = 0;

void sim_hard_disk(int msgid)
{
    message_t req;
    message_t ack;
    int msg_res = 0;
    struct timespec duration = {.tv_nsec = HD_ACCS_T, .tv_sec = 0};
    struct timespec remaining = {.tv_nsec = 0, .tv_sec = 0};

    memset(&req, 0, sizeof(req));
    memset(&ack, 0, sizeof(ack));

    while (1) {
        /* receive msg request */
        msg_res = msgrcv(msgid, &req, (sizeof(req) - sizeof(long)),
                            HD_REQUEST, 0);
        if (msg_res == -1 || msg_res < (sizeof(req) - sizeof(long))) {
            perror(
                "Error: Message receive failed in 'main thread' on process request.\n");
        }
        printf("Received request from %d with type %ld.\n", req.sender_id, req.msg_type);

        nanosleep(&duration, &remaining);

        ack.action = req.action;
        ack.sender_id = getpid();
        ack.msg_type = (long)(HD_ACK + req.action);
        
        msg_res = msgsnd(msgid, &ack, (sizeof(ack) - sizeof(long)), 0);
        if (msg_res == -1 || msg_res < (sizeof(ack) - sizeof(long))) {
            perror(
                "Error: Message sending failed in 'main thread' on request HD.\n");
        }
        printf("Sending ack to %d with type %ld.\n", req.sender_id, ack.msg_type);
    }
}

void *sim_mmu_main_thread(void *arg)
{
    int msgid = 0;
    msgid = *((int *)arg);
    mmu_main_loop(&mem_mutex, &cnt_mutex, &cond_mutex, memory, msgid,
                  &num_in_mem, &mmu_cond);
}

void *sim_mmu_evicter_thread(void *arg)
{
    int msgid = 0;
    msgid = *((int *)arg);
    mmu_evicter_loop(&mem_mutex, &cnt_mutex, &cond_mutex, memory, msgid,
                     &num_in_mem, &mmu_cond);
}

void *sim_mmu_printer_thread(void *arg)
{
    mmu_printer_loop(&mem_mutex, memory);
}

pid_t sim_my_fork(void)
{
    pid_t pid = 0;

    pid = fork();
    if (pid < 0) {
        perror("Error: Fork failed.\n");
        return -1;
    }
    return pid;
}

int main()
{
    pid_t pids[4];
    pthread_t mmu_main = NULL;
    pthread_t mmu_evicter = NULL;
    pthread_t mmu_printer = NULL;
    int msgid = 0;
    key_t key = 0;
    int i = 0;

    /* Initialization */
    memset(&pids, 0, sizeof(pids));

    /* Create message queue */
    key = ftok(".", 17);
    msgid = msgget(key, 0666 | IPC_CREAT);

    pids[0] = sim_my_fork();
    if (pids[0] == -1) {
        return 1;
    } else if (pids[0] == 0) {
        /* Child process -> MMU */
        /* Create threads */
        printf("MMU process with id %d starts...\n", getpid());
        pthread_create(&mmu_main, NULL, &sim_mmu_main_thread, (void *)(&msgid));
        pthread_create(&mmu_evicter, NULL, &sim_mmu_evicter_thread,
                       (void *)(&msgid));
        pthread_create(&mmu_printer, NULL, sim_mmu_printer_thread, NULL);
        pthread_join(mmu_main, NULL);
        pthread_join(mmu_evicter, NULL);
        pthread_join(mmu_printer, NULL);
        goto ok_ret;
    } else {
        pids[1] = sim_my_fork();
    }

    if(pids[1] == -1)
    {
        return 1;
    }
    else if (pids[1] == 0)
    {
        /* Child process -> MMU */
        /* Create Process 1 */
        printf("Process 1 with id %d starts...\n", getpid());
        run_process(msgid, getpid());
        goto ok_ret;
    } else {
        pids[2] = sim_my_fork();
    }

    if (pids[2] == -1) {
        return 1;
    } else if (pids[2] == 0) {
        /* Child process -> MMU */
        /* Create Process 2 */
        printf("Process 2 with id %d starts...\n", getpid());
        run_process(msgid, getpid());
        goto ok_ret;
    } else {
        pids[3] = sim_my_fork();
    }

    if (pids[3] == -1) {
        return 1;
    } else if (pids[3] == 0) {
        /* Child process -> MMU */
        /* Create Hard Disk */
        printf("HD process with id %d starts...\n", getpid());
        sim_hard_disk(msgid);
        goto ok_ret;
    }
    
    sleep(SIM_TIME);

    for (i = 0; i < 4; i++) {
        kill(pids[i], SIGKILL);
    }

    msgctl(msgid, IPC_RMID, NULL);

    printf("Successfully finished sim\n");
    
ok_ret:
    return 0;
}