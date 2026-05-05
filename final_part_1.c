#include "kilolib.h"

#define TOTAL_NUM 5

typedef enum{PHASE1, PHASE2} phase_t;

message_t msg;
uint8_t new_message;
uint8_t kilo_list[TOTAL_NUM] = {0, 0, 0, 0, 0};

uint8_t updated_list;

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
    msg.data[0] = kilo_uid;
    for(int i = 0; i < TOTAL_NUM; i++){
        msg.data[i+1] = kilo_list[i];
    }
}

message_t *message_tx(){
    return &msg;
}

void message_rx(message_t *m, distance_measurement_t *d){

}
