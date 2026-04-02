# include "kilolib.h"

#define N 2
#define LEADER_UID 10000

message_t msg_tx;
uint8_t message_sent = 0;
_Bool new_message = 0;

// store information about incoming messages
volatile uint8_t sender_id = 0;
volatile uint8_t dist_to_leader = 0; // lowkey redundent w measured distance, take average?
volatile uint8_t incoming_id = 0;
volatile uint8_t status = 0;
volatile uint8_t measured_distance = 0; // 

/* 
    status meanings:
    0: no assignemnts
    1: currently assigning a code
    2: someone has won the race, wait for new race
    3: id claiming broadcast "hey i won and i'm claiming an id"
    4: distance request by leader
    5: distance response
*/

uint8_t seed;
uint32_t timer_len = 0;

typedef enum{
    UNASSIGNED,
    RACING,
    ASSIGNED,
} bot_state;
bot_state current_state = UNASSIGNED;

typedef struct {
    uint8_t id;
    uint8_t distance;
} bot_dists_t;
bot_dists_t bot_dists[N];


// leader logic
uint8_t id_to_assign = 1;
uint8_t claimed_count = 0;
_Bool is_leader = 0;
_Bool all_assigned = 0;
_Bool pinging = 0;
_Bool resting_from_race = 0;
uint32_t rest_timer = 0;

// follower logic
uint8_t target_id = UINT8_MAX;
_Bool pending_status_reset = 0;

// all kilobots
uint8_t my_id = UINT8_MAX;



// message transmission
message_t *message_tx() {
    return &msg_tx;
}

void message_tx_success() {
    message_sent = 1; 

    // if a follower has just told everyone it successfully won a race, reset it to idle
    if(pending_status_reset){
        pending_status_reset = 0;
        msg_tx.data[8] = 0;
        msg_tx.crc = message_crc(&msg_tx);
    }
}

// recieving mesages
void message_rx(message_t *m, distance_measurement_t *d) {
    new_message = 1;
    
    // extract message data
    sender_id = m->data[0];
    dist_to_leader = m->data[1];
    incoming_id = m->data[7];
    status = m->data[8];

    // estimate distance
    if (sender_id == 0){
        measured_distance = estimate_distance(d); // only interested in distance from leader
    }
}

void setup(){
    msg_tx.type = NORMAL;
    msg_tx.crc = message_crc(&msg_tx);

    seed = rand_hard();
    rand_seed(seed);
    if (kilo_uid == LEADER_UID){
        is_leader = 1;
        my_id = 0;
        current_state = ASSIGNED; // Leader is assigned by default

        // init assignment message
        msg_tx.data[7] = id_to_assign;
        msg_tx.data[8] = 1;
        msg_tx.crc = message_crc(&msg_tx);

        // initialize bot_dists[i].id to all be uint8_max
        for (int i = 0; i < N; i++){
            bot_dists[i].id = UINT8_MAX;
        }
        // fill out first entry of bot_dists
        bot_dists[my_id].id = my_id;
        bot_dists[my_id].distance = 0;
    }
}

void loop(){
    // process new message
    if (new_message){
        
        // follower logic
        if(!is_leader){
            // start racing to claim id id not currently assigned one
            if(current_state == UNASSIGNED && status == 1){
                target_id = incoming_id;
                current_state = RACING;
                timer_len = kilo_ticks + (rand_soft() % 64);
            }
            // if already racing
            else if (current_state == RACING){
                // if someone else has won the race
                if (status == 2){
                    // concede defeat
                    current_state = UNASSIGNED;
                }
                else if (status == 1 && target_id != incoming_id){
                    // we missed the concede message
                    current_state = UNASSIGNED;
                }
            }
        }

        // leader logic
        if(is_leader){
            // check to see if someone has won the race
            if(!all_assigned && status == 3) {
                // make sure that the sender_id is within bounds and that this is the first time this kilobot has claimed
                // it has won a race. Guarding against multiple status 3s before successful reset
                if (sender_id >= 1 && sender_id < N  && bot_dists[sender_id].id == UINT8_MAX){
                    // verify that sender_id is within bounds
                    bot_dists[sender_id].id = sender_id;
                    bot_dists[sender_id].distance = dist_to_leader;
                    id_to_assign++;
                    claimed_count++;
                }

                // begin rest
                if (!resting_from_race){
                    resting_from_race = 1;
                    rest_timer = 16 + kilo_ticks;
                    msg_tx.data[8] = 2;
                    msg_tx.crc = message_crc(&msg_tx);
                }
                // check to see if all id's have now been assigned
                if(claimed_count >= N-1){
                    all_assigned = 1;
                    pinging = 1; // start pinging for distances
                    resting_from_race = 0; // cancel bc no more races
                }
            }
        }
        new_message = 0; // we finished processing the new message
    }


    // follower racing logic
    if (current_state == RACING){
        // check to see if we have reached the end
        if (kilo_ticks >= timer_len){
            // claim the id
            current_state = ASSIGNED;
            my_id = target_id;

            // tell everyone i won the race
            msg_tx.data[0] = my_id;
            msg_tx.data[1] = measured_distance;
            msg_tx.data[8] = 3; // i won the race
            msg_tx.crc = message_crc(&msg_tx);

            // ask for status reset
            pending_status_reset = 1;
        }
    }

    // leader broadcasting
    if (is_leader) {
        // if we are still conducting races
        if (!all_assigned){
            if(resting_from_race){
                msg_tx.data[8] = 2; // race is over, rest
                msg_tx.crc = message_crc(&msg_tx);

                if(kilo_ticks >= rest_timer){
                    // if we have rested enough, start next race
                    resting_from_race = 0;
                    msg_tx.data[7] = id_to_assign;
                    msg_tx.data[8] = 1;
                    msg_tx.crc = message_crc(&msg_tx);
                }
            } else{
                // keep broadcasting ongoing race
                msg_tx.data[7] = id_to_assign;
                msg_tx.data[8] = 1;
                msg_tx.crc = message_crc(&msg_tx);
            }
        } else if (pinging) {
            // if we are pinging for distances
            msg_tx.data[8] = 4; 
            msg_tx.crc = message_crc(&msg_tx);
        } else {
            // idle
            msg_tx.data[7] = UINT8_MAX; 
            msg_tx.data[8] = 0; 
            msg_tx.crc = message_crc(&msg_tx);
        }
    }

    //  color for debugging
    switch(my_id){
        case 0:
            // leader
            set_color(RGB(0, 0, 1)); // blue
            break;
        case 1:
            set_color(RGB(0, 1, 0)); // green
            break;
        case 2:
            set_color(RGB(1, 0, 0)); // red
            break;
        case 3:
            set_color(RGB(0, 1, 1)); // cyan
            break;
        case UINT8_MAX:
            set_color(RGB(0, 0, 0)); // no color
            break;
        default:
            set_color(RGB(0, 0, 0)); // no color
            break;
    }
}


int main() {
    kilo_init();
    kilo_message_tx = message_tx;
    kilo_message_tx_success = message_tx_success;
    kilo_message_rx = message_rx;
    kilo_start(setup, loop);

}
