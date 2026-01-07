#include "../include/proc.h"

void run_process(int msgid, int process_id)
{
    // define the struct of the message
    message_t my_message;
    long receive_type = 0;
    int send_result = 0;
    int recive_result = 0;

    memset(&my_message, 0, sizeof(my_message));

    // define varribls for probability
    double random_number;

    // define time cariabl for waiting
    struct timespec wait_time; // buld struct in time.h libarary

    wait_time.tv_sec = 0;
    wait_time.tv_nsec = INTER_MEM_ACCS_T;

    while (1) {
        // step 1 : waiting
        nanosleep(&wait_time, NULL);

        // step 2 : picking rsndom number between 0 to 1
        random_number = (double)rand() / RAND_MAX;

        // step 3 : sending the message to MMU
        my_message.msg_type = MMU_REQUEST; // type of message
        my_message.sender_id = process_id; // who send the message
        my_message.action = random_number < WR_RATE; // write or read

        send_result =
            msgsnd(msgid, &my_message, (sizeof(my_message) - sizeof(long)), 0);

        if (send_result == -1) {
            perror("ERROR:could not send message to NNU");
        }

        // step 4 : wait for acknowlegment from the MMU
        receive_type = process_id + (long)MMU_ACK;
        recive_result =
            msgrcv(msgid, &my_message, (sizeof(my_message) - sizeof(long)),
                   receive_type, 0);

        if (recive_result == -1) {
            perror("ERROR: could not recive acknowlegment from the MMU");
        }
    }
}
