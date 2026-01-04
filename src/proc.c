#include "../include/proc.h"


void run_process(int msgid , int process_id){

    // define the struct of the message
    struct msg_buffer my_message;

    //define varribls for probability

    double random_number;
    int is_it_a_write; 

    //define time cariabl for waiting

    struct timespec wait_time; // buld struct in time.h libarary 
    wait_time.tv_sec = 0;
    wait_time.tv_nsec = INTER_MEM_ACCS_T;

    while(1) { 
        // step 1 : waiting
        nanosleep(&wait_time, NULL);

        //step 2 : picking rsndom number between 0 to 1 
        random_number = (double)rand()/ (double)RAND_MAX; // RAND_MAX is the largest number that rand() can give

        if (random_number < WE_RATE){
            is_it_a_write = 1; // represnt write
        }
        else { 
            is_it_a_write = 0; // represent read
        }

        // step 3 : sending the message to MMU
        my_message.my_type = 1; // type of message
        my_message.sender_id = process_id ; // who send the message
        my_message.is_write = is_it_a_write; // write or read

        int send_result = msgsnd(msgid,&my_message,sizeof(my_message - sizeof(long)),0); 

        if (send_result == -1){
            perror("ERROR:could not send message to NNU");
            exit(1);
        }

        // step 4 : wait for acknowlegment from the MMU
        int receive_type = process_id + 10; 
        int recive_result = msgrcv(my_message,sizeof(my_message - sizeof(long)), receive_type, 0);

        if (recive_result == -1) { 
            perror("ERROR: could not recive acknowlegment from the MMU"); 
            exit(!);
        }
    }
}
