#include "kilolib.h"

#define STABLE_THRESHOLD 30        
#define FLASH_DURATION 320         
#define FLOOD_CYCLES 50           
#define MAX_UIDS 5
#define MAX_NEIGHBORS 4
#define NEIGHBOR_TIMEOUT 96       

typedef enum {
    GRAPH_MODE,
    LINE_CHECK,
    LINE_MODE,
    FLASHING,
    DRIVING_AWAY,
    ISOLATED
} robot_state_t;

typedef enum {
    MSG_UID_SET,
    MSG_NEIGHBOR_COUNT
} message_type_t;

typedef struct {
    uint16_t uid;
    uint8_t neighbor_count;
    uint8_t timestamp;
} neighbor_info_t;

robot_state_t mode;
uint16_t my_uid;
uint16_t known_uids[MAX_UIDS];
uint8_t known_uid_count;
uint8_t stable_count;
uint8_t prev_size;
uint8_t my_neighbor_count;
uint8_t flash_counter;
uint8_t flood_counter;
uint8_t current_tick;

neighbor_info_t neighbors[MAX_NEIGHBORS];
uint8_t neighbor_count;

typedef struct {
    uint8_t type;
    uint16_t uid;
    uint16_t uids[MAX_UIDS];
    uint8_t count;
    uint8_t neighbor_count;
} message_t;

message_t tx_message;

void add_uid_to_known(uint16_t uid) {
    uint8_t i;
    for (i = 0; i < known_uid_count; i++) {
        if (known_uids[i] == uid) return;
    }
    if (known_uid_count < MAX_UIDS) {
        known_uids[known_uid_count] = uid;
        known_uid_count++;
    }
}

void update_neighbor(uint16_t uid, uint8_t nc) {
    uint8_t i;
    for (i = 0; i < neighbor_count; i++) {
        if (neighbors[i].uid == uid) {
            neighbors[i].timestamp = current_tick;
            neighbors[i].neighbor_count = nc;
            return;
        }
    }
    if (neighbor_count < MAX_NEIGHBORS) {
        neighbors[neighbor_count].uid = uid;
        neighbors[neighbor_count].neighbor_count = nc;
        neighbors[neighbor_count].timestamp = current_tick;
        neighbor_count++;
    }
}

void prune_old_neighbors() {
    uint8_t i, j;
    for (i = 0; i < neighbor_count; i++) {
        if ((uint8_t)(current_tick - neighbors[i].timestamp) > NEIGHBOR_TIMEOUT) {
            for (j = i; j < neighbor_count - 1; j++) {
                neighbors[j] = neighbors[j + 1];
            }
            neighbor_count--;
            i--;
        }
    }
}

uint8_t count_direct_neighbors() {
    prune_old_neighbors();
    return neighbor_count;
}

void set_color_by_size(uint8_t size) {
    switch (size) {
        case 1: set_color(RGB(3, 3, 3)); break;      // White
        case 2: set_color(RGB(3, 0, 0)); break;      // Red
        case 3: set_color(RGB(3, 3, 0)); break;      // Yellow
        case 4: set_color(RGB(0, 3, 0)); break;      // Green
        case 5: set_color(RGB(0, 0, 3)); break;      // Blue
        default: set_color(RGB(0, 0, 0)); break;     
    }
}

void check_if_line() {
    uint8_t i;
    uint8_t max_neighbor_count;
    
    my_neighbor_count = count_direct_neighbors();
    max_neighbor_count = my_neighbor_count;
    
    for (i = 0; i < neighbor_count; i++) {
        if (neighbors[i].neighbor_count > max_neighbor_count) {
            max_neighbor_count = neighbors[i].neighbor_count;
        }
    }
    
    if (max_neighbor_count >= 3) {
        return;
    }
    
    mode = LINE_MODE;
    flood_counter = 0;
}

void message_rx(message_t *msg, distance_measurement_t *dist) {
    uint8_t i;
    
    if (msg->type == MSG_UID_SET) {
        for (i = 0; i < msg->count; i++) {
            add_uid_to_known(msg->uids[i]);
        }
        update_neighbor(msg->uid, 0);
    } 
    else if (msg->type == MSG_NEIGHBOR_COUNT) {
        update_neighbor(msg->uid, msg->neighbor_count);
    }
}

message_t *message_tx() {
    return &tx_message;
}

void message_tx_success() {
}

void setup() {
    mode = GRAPH_MODE;
    my_uid = kilo_uid;
    known_uid_count = 0;
    stable_count = 0;
    prev_size = 0;
    my_neighbor_count = 0;
    flash_counter = 0;
    flood_counter = 0;
    current_tick = 0;
    neighbor_count = 0;
    
    known_uids[0] = my_uid;
    known_uid_count = 1;
    prev_size = 1;
    
    set_color(RGB(3, 3, 3));  
}

void loop() {
    uint8_t graph_size;
    uint8_t i;
    
    current_tick++;
    
    switch (mode) {
        case GRAPH_MODE:
            tx_message.type = MSG_UID_SET;
            tx_message.uid = my_uid;
            tx_message.count = known_uid_count;
            for (i = 0; i < known_uid_count; i++) {
                tx_message.uids[i] = known_uids[i];
            }
            graph_size = known_uid_count;
           
            if (graph_size == prev_size) {
                stable_count++;
            } else {
                stable_count = 0;
                prev_size = graph_size;
            }
           
            if (stable_count >= STABLE_THRESHOLD) {
                set_color_by_size(graph_size);
                stable_count = 0;
                
                if (graph_size == 5) {
                    mode = LINE_CHECK;
                    flood_counter = 0;
                }
            }
            break;
        
        case LINE_CHECK:
            my_neighbor_count = count_direct_neighbors();
            tx_message.type = MSG_NEIGHBOR_COUNT;
            tx_message.uid = my_uid;
            tx_message.neighbor_count = my_neighbor_count;
            
            flood_counter++;
            if (flood_counter >= FLOOD_CYCLES) {
                check_if_line();
                if (mode == LINE_CHECK) {
                    mode = GRAPH_MODE;
                }
            }
            break;
        
        case LINE_MODE:
            my_neighbor_count = count_direct_neighbors();
            tx_message.type = MSG_NEIGHBOR_COUNT;
            tx_message.uid = my_uid;
            tx_message.neighbor_count = my_neighbor_count;
            
            if (my_neighbor_count >= 3) {
                mode = GRAPH_MODE;
                break;
            }
            
            if (my_neighbor_count <= 1) {
                if (known_uid_count == 1) {
                    set_color(RGB(0, 0, 3));  // Blue
                    mode = ISOLATED;
                } else {
                    mode = FLASHING;
                    flash_counter = 0;
                }
            }
            break;
        
        case FLASHING:
            if (flash_counter < FLASH_DURATION) {
                if ((flash_counter / 16) % 2 == 0) {
                    set_color(RGB(3, 3, 3));  // White
                } else {
                    set_color(RGB(0, 0, 0));  // Off
                }
                flash_counter++;
            } else {
                mode = DRIVING_AWAY;
                set_motors(kilo_straight_left, kilo_straight_right);
            }
            break;
        
        case DRIVING_AWAY:
            if (count_direct_neighbors() == 0) {
                set_motors(0, 0);
                known_uid_count = 1;
                known_uids[0] = my_uid;
                neighbor_count = 0;
                mode = GRAPH_MODE;
                stable_count = 0;
                prev_size = 1;
            }
            break;
        
        case ISOLATED:
            break;
    }
}

int main() {
    kilo_init();
    kilo_message_rx = message_rx;
    kilo_message_tx = message_tx;
    kilo_message_tx_success = message_tx_success;
    kilo_start(setup, loop);
    
    return 0;
}
