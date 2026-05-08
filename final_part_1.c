
#include "kilolib.h"

#define TOTAL_NUM 5
#define DEPENDENT_TIMEOUT 64 // ~2 sec

typedef enum { PHASE1, PHASE2 } phase_t;
phase_t current_phase;

// struct to track dependent kilobots and their last heard time
typedef struct {
    uint8_t name;
    uint32_t age;
} dependent_t;

/* manage new messages and local list of kilobots */
message_t msg;
uint8_t new_message;
uint8_t kilo_list[TOTAL_NUM] = {0, 0, 0, 0, 0};

/* kilobot to remove from global list as told by other kilobots */
uint8_t to_exile;
uint8_t revolution = 0;

/* manage received id and list being shared */
uint8_t rx_id;
uint8_t rx_list[TOTAL_NUM] = {0, 0, 0, 0, 0};

/* manage the kilobots that this bot was responsible
 * for adding, and removing them from the list
*/
dependent_t dependents[TOTAL_NUM] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}};

// kilobots that this kilobot added to the list and wants to remove
uint8_t disown = 0;

// blacklist to prevent re-adding exiled bots
uint8_t exile_blacklist[TOTAL_NUM] = {0, 0, 0, 0, 0};

void update_color(uint8_t kilo_count){
    switch (kilo_count)
    {
    case 1:
        set_color(RGB(1,1,1)); // white
        break;
    case 2:
        set_color(RGB(1, 0, 0)); // red
        break;
    case 3:
        set_color(RGB(0, 1, 1)); // cyan
        break;
    case 4:
        set_color(RGB(0, 1, 0)); // green
        break;
    case 5:
        set_color(RGB(0, 0, 1)); // blue
        break;
    default:
        set_color(RGB(1, 0, 1)); // magenta for troubleshooting
        break;
    }
}

void update_message() {
    msg.data[0] = kilo_uid;
    for(uint8_t i = 0; i < TOTAL_NUM; i++){
        msg.data[i+1] = kilo_list[i];
    }
    msg.data[6] = revolution;
    msg.data[7] = disown;
    msg.crc = message_crc(&msg);
}

message_t *message_tx(){
    return &msg;
}

void message_rx(message_t *m, distance_measurement_t *d){
    // reject messages with id 0 — 0 is the sentinel empty value
    if (m->data[0] == 0) return;

    rx_id = m->data[0];
    for(uint8_t i = 0; i < TOTAL_NUM; i++){
        rx_list[i] = m->data[i+1];
    }

    if (m->data[6] == 1){
        revolution = 1;
    }

    to_exile = m->data[7];

    new_message = 1;
}

void add_dependencies(){
    uint8_t already_dependent = 0;
    for(uint8_t i = 0; i < TOTAL_NUM; i++){
        if(dependents[i].name == rx_id) {
            already_dependent = 1;
            dependents[i].age = kilo_ticks;
            break;
        }
    }

    if(already_dependent == 0){
        for(uint8_t i = 0; i < TOTAL_NUM; i++){
            if(dependents[i].name == 0) {
                dependents[i].name = rx_id;
                dependents[i].age = kilo_ticks;
                break;
            }
        }
    }
}

// prevent disowned children from being added to the family again
void add_to_blacklist(uint8_t id){
    if (id == 0) return;
    for(uint8_t i = 0; i < TOTAL_NUM; i++){
        if(exile_blacklist[i] == id) return; // already blacklisted
    }
    for(uint8_t i = 0; i < TOTAL_NUM; i++){
        if(exile_blacklist[i] == 0){
            exile_blacklist[i] = id;
            return;
        }
    }
}

// check if child has been blacklisted
uint8_t is_blacklisted(uint8_t id){
    for(uint8_t i = 0; i < TOTAL_NUM; i++){
        if(exile_blacklist[i] == id) return 1;
    }
    return 0;
}

void remove_dependencies(){
    uint8_t exile_target = to_exile;
    to_exile = 0;

    // handle disowning of own child
    if (revolution == 1) {
        for(uint8_t i = 0; i < TOTAL_NUM; i++){
            if(dependents[i].name != 0) {
                // handle uint32 kilo_ticks wraparound
                if ((kilo_ticks - dependents[i].age) > DEPENDENT_TIMEOUT) {

                    disown = dependents[i].name;

                    // add to blacklist before removing
                    add_to_blacklist(dependents[i].name);

                    for(uint8_t j = 0; j < TOTAL_NUM; j++) {
                        if(kilo_list[j] == dependents[i].name) {
                            kilo_list[j] = 0; // remove from local list
                        }
                    }

                    dependents[i].name = 0;
                    dependents[i].age = 0;
                    break;
                }
            }
        }
    }

    // handle removing kilobots that have been expelled by other kilobots
    if (exile_target != 0) {
        // add to blacklist so we don't re-add it later by accident
        add_to_blacklist(exile_target);

        for(uint8_t i = 0; i < TOTAL_NUM; i++) {
            if(kilo_list[i] == exile_target) {
                kilo_list[i] = 0;
            }
            if(dependents[i].name == exile_target) {
                dependents[i].name = 0;
                dependents[i].age = 0;
            }
        }
    }
}

void validate_inclusion(){
    // do not re-add exiled bots
    if (is_blacklisted(rx_id)) return;

    uint8_t already_in = 0;
    for(uint8_t i = 0; i < TOTAL_NUM; i++){
        if(kilo_list[i] == rx_id) already_in = 1;
    }

    if(already_in == 0){
        for(uint8_t i = 0; i < TOTAL_NUM; i++){
            if(kilo_list[i] == 0){
                kilo_list[i] = rx_id;
                break;
            }
        }
    }

    for(uint8_t i = 0; i < TOTAL_NUM; i++){
        if (rx_list[i] == 0) continue;

        // skip any entries in the received list that are blacklisted
        if (is_blacklisted(rx_list[i])) continue;

        already_in = 0;
        for(uint8_t j = 0; j < TOTAL_NUM; j++){
            if(kilo_list[j] == rx_list[i]) {
                already_in = 1;
                break;
            }
        }

        if(already_in == 0){
            for(uint8_t k = 0; k < TOTAL_NUM; k++){
                if (kilo_list[k] == 0) {
                    kilo_list[k] = rx_list[i];
                    break;
                }
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

void setup() {
    msg.type = NORMAL;
    current_phase = PHASE1;

    for (int i = 0; i < TOTAL_NUM; i++) {
        kilo_list[i] = 0;
        exile_blacklist[i] = 0;
    }
    kilo_list[0] = kilo_uid;
}

void loop(){
    uint8_t current_size = check_global_size();

    if(current_phase == PHASE1){
        if(current_size >= TOTAL_NUM){
            revolution = 1;
        }

        update_message();

        // reset disown after it has been packed into the outgoing message
        // so it broadcasts for exactly one message cycle and stops
        disown = 0;

        update_color(current_size);

        if(new_message){
            new_message = 0;
            add_dependencies();
            validate_inclusion();
        }
        remove_dependencies();

        // transition to PHASE2 when the count returns to 1
        // (revolution completed — all other bots have been exiled)
        if(revolution == 1 && current_size == 1){
            current_phase = PHASE2;
        }

    } else { // phase 2

    }
}

int main(){
    kilo_init();
    kilo_message_rx = message_rx;
    kilo_message_tx = message_tx;
    kilo_start(setup, loop);
    return 0;
}

