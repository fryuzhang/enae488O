#include "kilolib.h"

/* LETS CODE SOME KILOBOTS


/* Total number of kilobots expected in the group. */
#define TOTAL_NUM 5

/* Time allowed before a previously heard robot is considered missing. */
#define DEPENDENT_TIMEOUT 2 * TICKS_PER_SEC

/* Time after seeing the full group before revolution/removal behavior starts. */
#define REVOLUTION_START_TIME 4 * TICKS_PER_SEC

/* Time without hearing messages before switching behavior. */
#define ISOLATION_TIMEOUT 4 * TICKS_PER_SEC

/* Time to keep broadcasting a removed robot ID. */
#define DISOWN_BROADCAST_TIME 2 * TICKS_PER_SEC

/* Time each selected group flashes white before moving. */
#define FLASH_TIME 10 * TICKS_PER_SEC

/* Blink period for clear flashing LEDs. */
#define FLASH_BLINK_PERIOD (TICKS_PER_SEC / 2)

/* Motion commands. */
#define STOP    0
#define FORWARD 1
#define LEFT    2
#define RIGHT   3

/* Main behavior phases. */
typedef enum { PHASE1, PHASE2 } phase_t;
phase_t current_phase;

/* Stores a robot ID and the last time it was heard. */
typedef struct {
    uint8_t name;
    uint32_t age;
} dependent_t;

/* Outgoing message. */
message_t msg;

/* Flag set when a new message is received. */
uint8_t new_message;

/* Local list of known kilobot IDs. */
uint8_t kilo_list[TOTAL_NUM] = {0, 0, 0, 0, 0};

/* ID received from another robot telling this robot who to remove. */
uint8_t to_exile;

/* Flag used to begin removal behavior after full group detection. */
uint8_t revolution = 0;

/* Most recently received robot ID. */
uint8_t rx_id;

/* Most recently received robot list from another kilobot. */
uint8_t rx_list[TOTAL_NUM] = {0, 0, 0, 0, 0};

/* Recently heard neighbors and their last-heard times. */
dependent_t dependents[TOTAL_NUM] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}};

/* Robot ID this robot wants others to remove. */
uint8_t disown = 0;

/* Time when the current disown message started broadcasting. */
uint32_t disown_start_tick = 0;

/* IDs that should not be re-added after removal. */
uint8_t exile_blacklist[TOTAL_NUM] = {0, 0, 0, 0, 0};

/* Flag set when this robot has been told it is removed. */
uint8_t is_exiled = 0;

/* Flag for a robot selected as an end robot. Currently only used as an override. */
uint8_t is_end = 0;

/* Time when this robot first observed the full group. */
uint32_t full_size_start_tick = 0;

/* Last time any message was heard. */
uint32_t last_heard_tick = 0;

/* Time when the white-flash/movement sequence begins. */
uint32_t flash_start_tick = 0;

/* Flag that starts the line-breaking sequence in Phase 2. */
uint8_t break_line = 0;

/* Current motion command. Used to avoid repeatedly resetting motors. */
uint8_t cur_motion = STOP;

/* Changes motor state only when the requested motion changes. */
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

/* Sets LED color based on how many robots are currently known. */
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

/* Makes the LED visibly blink white/off. */
void flash_white_clear(){
    if(((kilo_ticks / FLASH_BLINK_PERIOD) % 2) == 0){
        set_color(RGB(1, 1, 1));
    } else {
        set_color(RGB(0, 0, 0));
    }
}

/* Makes the LED visibly blink blue/off. */
void flash_blue_clear(){
    if(((kilo_ticks / FLASH_BLINK_PERIOD) % 2) == 0){
        set_color(RGB(0, 0, 1));
    } else {
        set_color(RGB(0, 0, 0));
    }
}

/* Updates the outgoing message with this robot's ID, known list, revolution flag, and disown ID. */
void update_message() {
    /* Stop broadcasting the disown ID after the broadcast window expires. */
    if(disown != 0 && (kilo_ticks - disown_start_tick) > DISOWN_BROADCAST_TIME){
        disown = 0;
    }

    msg.data[0] = kilo_uid;

    /* Broadcast this robot's known list of kilobot IDs. */
    for(uint8_t i = 0; i < TOTAL_NUM; i++){
        msg.data[i+1] = kilo_list[i];
    }

    msg.data[6] = revolution;
    msg.data[7] = disown;
    msg.crc = message_crc(&msg);
}

/* Kilobot transmit callback. */
message_t *message_tx(){
    return &msg;
}

/* Kilobot receive callback. Stores the latest received packet. */
void message_rx(message_t *m, distance_measurement_t *d){
    if (m->data[0] == 0) return;

    rx_id = m->data[0];

    /* Copy the sender's known robot list. */
    for(uint8_t i = 0; i < TOTAL_NUM; i++){
        rx_list[i] = m->data[i+1];
    }

    /* If any robot says revolution started, this robot also starts it. */
    if (m->data[6] == 1){
        revolution = 1;
    }

    /* Store any received removal request. */
    to_exile = m->data[7];

    new_message = 1;
}

/* Tracks the most recently heard sender as a dependent/neighbor. */
void add_dependencies(){
    uint8_t already_dependent = 0;

    /* Refresh the timestamp if this sender is already known. */
    for(uint8_t i = 0; i < TOTAL_NUM; i++){
        if(dependents[i].name == rx_id) {
            already_dependent = 1;
            dependents[i].age = kilo_ticks;
            break;
        }
    }

    /* Add the sender if there is an empty dependent slot. */
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

/* Adds an ID to the blacklist so it does not get re-added later. */
void add_to_blacklist(uint8_t id){
    if (id == 0) return;

    /* Do not add duplicates. */
    for(uint8_t i = 0; i < TOTAL_NUM; i++){
        if(exile_blacklist[i] == id) return;
    }

    /* Add to the first open slot. */
    for(uint8_t i = 0; i < TOTAL_NUM; i++){
        if(exile_blacklist[i] == 0){
            exile_blacklist[i] = id;
            return;
        }
    }
}

/* Checks whether an ID has been blacklisted. */
uint8_t is_blacklisted(uint8_t id){
    for(uint8_t i = 0; i < TOTAL_NUM; i++){
        if(exile_blacklist[i] == id) return 1;
    }
    return 0;
}

/* Removes missing robots and applies received removal requests. */
void remove_dependencies(){
    uint8_t exile_target = to_exile;
    to_exile = 0;

    /* After revolution starts, remove robots that have timed out. */
    if (revolution == 1) {
        for(uint8_t i = 0; i < TOTAL_NUM; i++){
            if(dependents[i].name != 0) {
                if ((kilo_ticks - dependents[i].age) > DEPENDENT_TIMEOUT) {

                    /* Broadcast this removed ID for a short time. */
                    disown = dependents[i].name;
                    disown_start_tick = kilo_ticks;

                    add_to_blacklist(dependents[i].name);

                    /* Remove the timed-out ID from the local known list. */
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

    /* Apply an incoming removal request from another robot. */
    if (exile_target != 0) {

        /* If this robot is the target, mark itself exiled. */
        if (exile_target == kilo_uid) {
            is_exiled = 1;
            return;
        }

        /* Relay the removal message if this ID was not already known as removed. */
        if(!is_blacklisted(exile_target)){
            disown = exile_target;
            disown_start_tick = kilo_ticks;
        }

        add_to_blacklist(exile_target);

        /* Remove the target ID from local list and dependent table. */
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

/* Adds received robot IDs into this robot's local known list. */
void validate_inclusion(){
    if (is_blacklisted(rx_id)) return;

    uint8_t already_in = 0;

    /* Add the sender ID if it is not already in this robot's list. */
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

    /* Merge the sender's known list into this robot's known list. */
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

/* Counts how many nonzero robot IDs are in this robot's local known list. */
uint8_t check_global_size(){
    uint8_t global_count = 0;

    for(uint8_t i = 0; i < TOTAL_NUM; i++){
        if (kilo_list[i] != 0) global_count++;
    }

    return global_count;
}

/* Initializes state before the Kilobot main loop starts. */
void setup() {
    msg.type = NORMAL;
    current_phase = PHASE1;

    is_exiled = 0;
    full_size_start_tick = 0;
    last_heard_tick = 0;
    disown_start_tick = 0;
    flash_start_tick = 0;
    break_line = 0;
    cur_motion = STOP;

    /* Clear all local memory arrays. */
    for (int i = 0; i < TOTAL_NUM; i++) {
        kilo_list[i] = 0;
        rx_list[i] = 0;
        exile_blacklist[i] = 0;
        dependents[i].name = 0;
        dependents[i].age = 0;
    }

    /* Each robot begins by knowing only itself. */
    kilo_list[0] = kilo_uid;
}

/* Main behavior loop. */
void loop(){

    /* Exiled robots stop and stay white. */
    if (is_exiled) {
        set_motion(STOP);
        set_color(RGB(1, 1, 1));
        return;

    /* End flag override. Currently this is not actively used in the break-line logic. */
    } else if (is_end) {
        set_motion(STOP);
        set_color(RGB(0, 1, 1));
        return;
    }

    if(current_phase == PHASE1){

        /* Process the latest received message. */
        if(new_message){
            new_message = 0;
            last_heard_tick = kilo_ticks;
            add_dependencies();
            validate_inclusion();
        }

        /* Remove timed-out or explicitly disowned robots. */
        remove_dependencies();

        uint8_t current_size = check_global_size();

        /* Once all robots are detected, start timing the revolution/removal trigger. */
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

        /* If isolated after revolution starts, transition into Phase 2. */
        if (revolution == 1 && last_heard_tick != 0 &&
            (kilo_ticks - last_heard_tick) >= ISOLATION_TIMEOUT) {

            set_motion(STOP);
            set_color(RGB(1, 1, 0));
            current_phase = PHASE2;
            
            /* Reset communication/list state for Phase 2. */
            full_size_start_tick = 0;
            last_heard_tick = 0;
            disown_start_tick = 0;
            to_exile = 0;
            disown = 0;
            new_message = 0;
            revolution = 0;
            break_line = 0;
            flash_start_tick = 0;

            for (int i = 0; i < TOTAL_NUM; i++) {
                kilo_list[i] = 0;
                rx_list[i] = 0;
                exile_blacklist[i] = 0;
                dependents[i].name = 0;
                dependents[i].age = 0;
            }

            /* Restart Phase 2 list-building from only this robot. */
            kilo_list[0] = kilo_uid;

            update_message();
        }

    } else {
        
        /* Phase 2: rebuild list, then execute the staged break-line sequence. */

        if(new_message){
            new_message = 0;
            last_heard_tick = kilo_ticks;
            add_dependencies();
            validate_inclusion();
        }

        remove_dependencies();

        uint8_t current_size = check_global_size();

        update_message();

        /* Once the break sequence has started, the last remaining robot flashes blue. */
        if (break_line > 0 && current_size == 1) {
            set_motion(STOP);
            flash_blue_clear();
            return;
        } else {
            update_color(current_size);
        }

        /* Wait until the full Phase 2 group is rebuilt, then start the break-line timer. */
        if (current_size >= TOTAL_NUM) {
            if (full_size_start_tick == 0) {
                full_size_start_tick = kilo_ticks;
            }
            if ((kilo_ticks - full_size_start_tick) >= REVOLUTION_START_TIME && break_line == 0) {
                revolution = 1;
                break_line = 1;
                flash_start_tick = kilo_ticks;
            }
        }

        /*
         * Phase 2 isolation reset.
         * The break_line == 0 guard prevents this reset from interrupting
         * the 10-second flash-then-drive sequence.
         */
        if (break_line == 0 && revolution == 1 && last_heard_tick != 0 &&
            (kilo_ticks - last_heard_tick) >= ISOLATION_TIMEOUT) {

            set_motion(STOP);
            set_color(RGB(1, 1, 0));
            current_phase = PHASE2;
            
            full_size_start_tick = 0;
            last_heard_tick = 0;
            disown_start_tick = 0;
            to_exile = 0;
            disown = 0;
            new_message = 0;
            revolution = 0;
            break_line = 0;
            flash_start_tick = 0;

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

        /* Staged movement sequence after break_line starts. */
        if (break_line > 0) {
            uint32_t elapsed_time = kilo_ticks - flash_start_tick;

            /*
             * Robots 1 and 2:
             * flash white for 10 seconds, then drive forward.
             */
            if (kilo_uid == 1 || kilo_uid == 2) {
                if (elapsed_time < FLASH_TIME) {
                    set_motion(STOP);
                    flash_white_clear();
                } else {
                    set_motion(FORWARD);
                }
            }

            /*
             * Robots 3 and 4:
             * wait during the first flash period,
             * then flash white for 10 seconds,
             * then drive forward.
             */
            if (kilo_uid == 3 || kilo_uid == 4) {
                if (elapsed_time >= FLASH_TIME && elapsed_time < (FLASH_TIME * 2)) {
                    set_motion(STOP);
                    flash_white_clear();
                } else if (elapsed_time >= (FLASH_TIME * 2)) {
                    set_motion(FORWARD);
                } else {
                    set_motion(STOP);
                }
            }

            /*
             * Robot 5:
             * stays still as the final robot.
             * It flashes blue only when the local list says it is alone.
             */
            if (kilo_uid == 5) {
                set_motion(STOP);
            }

        } else {
            /* Before break_line starts, all robots stay stopped. */
            set_motion(STOP);
        }
    }
}

/* Kilobot program entry point. */
int main(){
    kilo_init();
    kilo_message_rx = message_rx;
    kilo_message_tx = message_tx;
    kilo_start(setup, loop);
    return 0;
}
