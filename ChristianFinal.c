#include "kilolib.h"

#define TOTAL_NUM 5
#define DEPENDENT_TIMEOUT 4 * TICKS_PER_SEC
#define REVOLUTION_START_TIME 6 * TICKS_PER_SEC
#define ISOLATION_TIMEOUT 4 * TICKS_PER_SEC  // self-exile if no message heard for this long
#define DISOWN_BROADCAST_TIME 4 * TICKS_PER_SEC

#define STOP    0
#define FORWARD 1
#define LEFT    2
#define RIGHT   3

#define PHASE2_FLASH_TIME 10 * TICKS_PER_SEC
#define PHASE2_OUT_OF_RANGE_TIME 2 * TICKS_PER_SEC

typedef enum { PHASE1, PHASE2 } phase_t;
phase_t current_phase;

typedef enum { P2_WAITING, P2_FLASHING, P2_DRIVING, P2_GONE, P2_LAST } phase2_state_t;
phase2_state_t phase2_state;

typedef struct {
    uint8_t name;
    uint32_t age;
} dependent_t;

message_t msg;
uint8_t new_message;
uint8_t kilo_list[TOTAL_NUM] = {0, 0, 0, 0, 0};

uint8_t to_exile;
uint8_t revolution = 0;

uint8_t rx_id;
uint8_t rx_list[TOTAL_NUM] = {0, 0, 0, 0, 0};

dependent_t dependents[TOTAL_NUM] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}};

uint8_t disown = 0;
uint32_t disown_start_tick = 0;

uint8_t exile_blacklist[TOTAL_NUM] = {0, 0, 0, 0, 0};

// flag set when this bot learns it has been exiled
uint8_t is_exiled = 0;
uint8_t is_end = 0;

// tick when this bot first observed a full-size swarm
uint32_t full_size_start_tick = 0;

// last tick this bot heard any message from a neighbor
uint32_t last_heard_tick = 0;

uint8_t cur_motion = STOP;
uint32_t phase2_flash_start_tick = 0;
uint32_t phase2_no_neighbor_start_tick = 0;

void set_motion(uint8_t new_motion)
{
    if (cur_motion != new_motion) {
        cur_motion = new_motion;

        switch (cur_motion) {
            case STOP:
                set_motors(0, 0);
                break;

            case FORWARD:
                spinup_motors();
                set_motors(kilo_straight_left, kilo_straight_right);
                break;

            case LEFT:
                spinup_motors();
                set_motors(kilo_turn_left, 0);
                break;

            case RIGHT:
                spinup_motors();
                set_motors(0, kilo_turn_right);
                break;
        }
    }
}

void update_color(uint8_t kilo_count){
    // if (revolution){
    //     set_color(RGB(1, 0, 1));
    //     return;
    // }
    switch (kilo_count)
    {
    case 1:
        set_color(RGB(1,1,1)); // white
        break;
    case 2:
        set_color(RGB(1, 0, 0)); // red
        break;
    case 3:
        set_color(RGB(1, 1, 0)); // yellow
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
    if(disown != 0 && (kilo_ticks - disown_start_tick) > DISOWN_BROADCAST_TIME){
        disown = 0;
    }

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

void add_to_blacklist(uint8_t id){
    if (id == 0) return;
    for(uint8_t i = 0; i < TOTAL_NUM; i++){
        if(exile_blacklist[i] == id) return;
    }
    for(uint8_t i = 0; i < TOTAL_NUM; i++){
        if(exile_blacklist[i] == 0){
            exile_blacklist[i] = id;
            return;
        }
    }
}

uint8_t is_blacklisted(uint8_t id){
    for(uint8_t i = 0; i < TOTAL_NUM; i++){
        if(exile_blacklist[i] == id) return 1;
    }
    return 0;
}

void remove_dependencies(){
    uint8_t exile_target = to_exile;
    to_exile = 0;

    if (revolution == 1) {
        for(uint8_t i = 0; i < TOTAL_NUM; i++){
            if(dependents[i].name != 0) {
                if ((kilo_ticks - dependents[i].age) > DEPENDENT_TIMEOUT) {
                    disown = dependents[i].name;
                    disown_start_tick = kilo_ticks;

                    add_to_blacklist(dependents[i].name);

                    for(uint8_t j = 0; j < TOTAL_NUM; j++) {
                        if(kilo_list[j] == dependents[i].name) {
                            kilo_list[j] = 0;
                        }
                    }

                    dependents[i].name = 0;
                    dependents[i].age = 0;
                    break;
                }
            }
        }
    }

    if (exile_target != 0) {
        // check if this bot itself has been exiled
        if (exile_target == kilo_uid) {
            is_exiled = 1;
            return;
        }

        if(!is_blacklisted(exile_target)){
            disown = exile_target;
            disown_start_tick = kilo_ticks;
        }

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

void phase2_flash_white(){
    if(((kilo_ticks / (TICKS_PER_SEC / 2)) % 2) == 0){
        set_color(RGB(1, 1, 1));
    } else {
        set_color(RGB(0, 0, 0));
    }
}

void phase2_flash_blue(){
    if(((kilo_ticks / (TICKS_PER_SEC / 2)) % 2) == 0){
        set_color(RGB(0, 0, 1));
    } else {
        set_color(RGB(0, 0, 0));
    }
}

uint8_t phase2_id_active(uint8_t id){
    if(id == 0) return 0;
    if(is_blacklisted(id)) return 0;

    for(uint8_t i = 0; i < TOTAL_NUM; i++){
        if(kilo_list[i] == id){
            return 1;
        }
    }

    return 0;
}

uint8_t phase2_count_neighbors(){
    uint8_t count = 0;

    for(uint8_t i = 0; i < TOTAL_NUM; i++){
        uint8_t id = dependents[i].name;

        if(id == 0) continue;
        if(id == kilo_uid) continue;
        if(is_blacklisted(id)) continue;
        if(!phase2_id_active(id)) continue;

        if((kilo_ticks - dependents[i].age) <= DEPENDENT_TIMEOUT){
            count++;
        }
    }

    return count;
}

uint8_t phase2_lowest_active_uid(){
    uint8_t lowest = 0;

    for(uint8_t i = 0; i < TOTAL_NUM; i++){
        uint8_t id = kilo_list[i];

        if(id == 0) continue;
        if(is_blacklisted(id)) continue;

        if(lowest == 0 || id < lowest){
            lowest = id;
        }
    }

    return lowest;
}

uint8_t phase2_is_end_robot(){
    uint8_t current_size = check_global_size();
    uint8_t neighbor_count = phase2_count_neighbors();

    if(current_size <= 1) return 0;

    /*
     * Normal line case:
     * An end robot hears exactly one active neighbor.
     */
    if(current_size > 2 && neighbor_count == 1){
        return 1;
    }

    /*
     * Safety case:
     * If only two robots are left, both are technically ends.
     * Only the lower UID drives away so one robot remains.
     */
    if(current_size == 2 && neighbor_count == 1){
        if(kilo_uid == phase2_lowest_active_uid()){
            return 1;
        }
    }

    return 0;
}

void phase2_remove_self_from_list(){
    for(uint8_t i = 0; i < TOTAL_NUM; i++){
        if(kilo_list[i] == kilo_uid){
            kilo_list[i] = 0;
        }
    }
}

void setup() {
    msg.type = NORMAL;
    current_phase = PHASE1;
    phase2_state = P2_WAITING;

    is_exiled = 0;
    full_size_start_tick = 0;
    last_heard_tick = 0;
    disown_start_tick = 0;

    cur_motion = STOP;
    phase2_flash_start_tick = 0;
    phase2_no_neighbor_start_tick = 0;

    for (int i = 0; i < TOTAL_NUM; i++) {
        kilo_list[i] = 0;
        exile_blacklist[i] = 0;
    }
    kilo_list[0] = kilo_uid;
}

void loop(){
    // exiled bots freeze as white and do nothing else
    if (is_exiled) {
        set_color(RGB(1, 1, 1));
        return;
    } else if (is_end) {
        set_color(RGB(0, 1, 1));
        return;
    }

    if(current_phase == PHASE1){

        if(new_message){
            new_message = 0;
            last_heard_tick = kilo_ticks;  // refresh timestamp on every received message
            add_dependencies();
            validate_inclusion();
        }

        remove_dependencies();

        uint8_t current_size = check_global_size();

        if (current_size >= TOTAL_NUM) {
            if (full_size_start_tick == 0) {
                full_size_start_tick = kilo_ticks;
            }
            if ((kilo_ticks - full_size_start_tick) >= REVOLUTION_START_TIME) {
                revolution = 1;
            }
        }

        update_message();
        update_color(current_size);

        // if isolated after revolution, switch into phase 2
        if (revolution == 1 && last_heard_tick != 0 &&
            (kilo_ticks - last_heard_tick) >= ISOLATION_TIMEOUT) {
            set_color(RGB(1, 1, 0)); // yellow — I am isolated
            current_phase = PHASE2;
            phase2_state = P2_WAITING;
            set_motion(STOP);
            
            // reset add-code variables for phase 2
            full_size_start_tick = 0;
            last_heard_tick = 0;
            disown_start_tick = 0;
            to_exile = 0;
            disown = 0;
            new_message = 0;
            phase2_flash_start_tick = 0;
            phase2_no_neighbor_start_tick = 0;

            for (int i = 0; i < TOTAL_NUM; i++) {
                kilo_list[i] = 0;
                rx_list[i] = 0;
                exile_blacklist[i] = 0;
                dependents[i].name = 0;
                dependents[i].age = 0;
            }
            kilo_list[0] = kilo_uid;

            update_message();
        }

        // if(revolution == 1 && current_size == 1){
        //     current_phase = PHASE2;
        // }

    } else { // phase 2

        if(new_message){
            new_message = 0;
            last_heard_tick = kilo_ticks;

            if(phase2_state != P2_GONE){
                add_dependencies();
                validate_inclusion();
            }
        }

        /*
         * While driving away or gone, this robot should not run the normal
         * remove logic on itself. Remaining robots still run removal.
         */
        if(phase2_state != P2_DRIVING && phase2_state != P2_GONE){
            remove_dependencies();
        }

        uint8_t current_size = check_global_size();

        if(phase2_state == P2_GONE){
            set_motion(STOP);
            set_color(RGB(0, 0, 0));
            update_message();
            return;
        }

        if(current_size == 1 && phase2_id_active(kilo_uid)){
            phase2_state = P2_LAST;
        }

        if(phase2_state == P2_LAST){
            set_motion(STOP);
            phase2_flash_blue();
            update_message();
            return;
        }

        if(phase2_state == P2_FLASHING){
            set_motion(STOP);

            if((kilo_ticks - phase2_flash_start_tick) >= PHASE2_FLASH_TIME){
                phase2_state = P2_DRIVING;
                phase2_no_neighbor_start_tick = 0;
                set_motion(FORWARD);
            } else {
                phase2_flash_white();
            }

            update_message();
            return;
        }

        if(phase2_state == P2_DRIVING){
            set_motion(FORWARD);

            if(phase2_count_neighbors() == 0){
                if(phase2_no_neighbor_start_tick == 0){
                    phase2_no_neighbor_start_tick = kilo_ticks;
                }

                if((kilo_ticks - phase2_no_neighbor_start_tick) >= PHASE2_OUT_OF_RANGE_TIME){
                    set_motion(STOP);
                    phase2_state = P2_GONE;
                    phase2_remove_self_from_list();
                    set_color(RGB(0, 0, 0));
                }
            } else {
                phase2_no_neighbor_start_tick = 0;
            }

            update_message();
            return;
        }

        if(phase2_is_end_robot()){
            phase2_state = P2_FLASHING;
            phase2_flash_start_tick = kilo_ticks;
            set_motion(STOP);
            phase2_flash_white();
        } else {
            set_motion(STOP);
            update_color(current_size);
        }

        update_message();
    }
}

int main(){
    kilo_init();
    kilo_message_rx = message_rx;
    kilo_message_tx = message_tx;
    kilo_start(setup, loop);
    return 0;
}
