#include "kilolib.h"

#define TOTAL_NUM 5

typedef enum{PHASE1, PHASE2} phase_t;

/* Here local refers to list managed by the individual kilobot, 
 * not kilobots that it can directly communicate with 
*/

/* Manage new messages and local list of kilobots */
message_t msg;
uint8_t new_message;
uint8_t kilo_list[TOTAL_NUM] = {0, 0, 0, 0, 0};

/* 
 * Kilobot to remove from global list as told by
 * other kilobots, kilobots can only be removed after 
 * we have reached 5 total kilobots in the global list
*/
uint8_t to_exile;
uint8_t revolution = 0;

/*
 * Manage recieved id and list being shared by
 * other kilobots
*/
uint8_t rx_id;
uint8_t rx_list[TOTAL_NUM] = {0, 0, 0, 0, 0};

/* Manage the kilobots that this bot was responsible
 * for adding, and removing them from the list
*/
uint8_t dependents[TOTAL_NUM] = {0, 0, 0, 0, 0};
// kilobots that this kilobot added to the list
uint8_t disown;

void update_color(kilo_count){
    // red yellow green blue
    // the kilobot includes itself in its count
    switch (kilo_count)
    {
    case 1:
        set_color(RGB(1,1,1)); // white
        break; 
    case 2:
        set_color(RGB(1, 0, 0));
        break;
    case 3:
        set_color(RGB(0, 1, 1));
        break;
    case 4:
        set_color(RGB(0, 1, 0));
        break;
    case 5:
        set_color(RGB(0, 0, 1));
    default:
        set_color(RGB(1, 0, 1)); // magenta for trouble shooting
        break;
    }
}

void update_message() {
    // set message to be sent

    /* 
     * Send the id of the current bot,
     * The list of id's in it's current list
     * the id of a kilbot to be disowned
    */
    msg.data[0] = kilo_uid;
    for(uint8_t i = 0; i < TOTAL_NUM; i++){
        msg.data[i+1] = kilo_list[i];
    }

    msg.data[7] = disown;
}

message_t *message_tx(){
    return &msg;
}

void message_rx(message_t *m, distance_measurement_t *d){
    rx_id = m->data[0];
    for(uint8_t i = 0; i < TOTAL_NUM; i++){
        // populate rx_list buffer to update local list
        rx_list[i] = m->data[i+1];
    }

    // if exiled, remove from local list
    to_exile = m->data[7]; 
}

void add_dependencies(){
    uint8_t new_dependent = 1;
    // check if rx_id is already a dependent
    // if it's not a dependent, and the current index is
    // avaliable, add it as a dependent
    for(uint8_t i = 0; i < TOTAL_NUM; i++){
        if(dependents[i] == rx_id) new_dependent = 0;
        if(new_dependent == 1 && dependents[i] == 0) dependents[i] = rx_id;
    }
}

void remove_dependencies(){
    // if we haven't heard from a dependent for 2 seconds
    // remove it as a depend and tell all kilobots to remove
    // it from the global list

}


void validate_inclusion(){
    // check if the recieved id is in the local list
    // if it's not add it to the end
    uint8_t already_in = 1;
    for(uint8_t i = 0; i < TOTAL_NUM; i++){
        if(kilo_list[i] == rx_id) already_in = 0;
        if(kilo_list[i] == 0 && already_in == 0){
            kilo_list[i] = rx_id;
        }
    }


    // then check if the recieved list matches the local list
    // if recieved list has new entries, add it to the local
    // list. 
    uint8_t missing_id_list[TOTAL_NUM];
    uint8_t missing_count = 0;
    for(uint8_t i = 0; i < TOTAL_NUM; i++){
        already_in = 1;
        for(uint8_t j = 0; i < TOTAL_NUM; i++){
            if(kilo_list[j] != rx_list[i]) already_in = 0;
            else already_in = 1;
        }
        if(already_in == 0){
            for(uint8_t k = 0; i < TOTAL_NUM; i++){
                if (missing_id_list[k] == 0) missing_id_list[k] = rx_list[i];
            }
            missing_count++;
        }
    }

    // populate the current list with all missing values
    for(uint8_t i = 0; i < missing_count; i++){
        if(missing_id_list[i] != 0){
            for(uint8_t j = 0; i < TOTAL_NUM; i++){
                if (kilo_list[j] == 0) kilo_list[j] = missing_id_list[i];
            }
        }
    }
}

uint8_t check_global_size(){
    uint8_t global_count = 0;
    for(uint8_t i = 0; i < TOTAL_NUM; i++){
        if (kilo_list[i] != 0) global_count++;
    }
    return global_count;
}
