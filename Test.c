#include <kilolib.h>

// Configuration constants
#define STABLE_THRESHOLD 30
#define FLASH_DURATION 320
#define FLOOD_CYCLES 50
#define MAX_UIDS 5
#define MAX_NEIGHBORS 4
#define NEIGHBOR_TIMEOUT 96

// Robot states
#define GRAPH_MODE 0
#define LINE_CHECK 1
#define LINE_MODE 2
#define FLASHING 3
#define DRIVING_AWAY 4
#define ISOLATED 5

// Message types
#define MSG_UID_SET 0
#define MSG_NEIGHBOR_COUNT 1

// Neighbor tracking structure
typedef struct {
    uint16_t uid;
    uint8_t neighbor_count;
    uint8_t timestamp;
} neighbor_info_t;

// Global state variables
uint8_t mode;
uint16_t my_uid;
uint16_t known_uids[MAX_UIDS];
uint8_t known_uid_count;
uint8_t stable_count;
uint8_t prev_size;
uint8_t my_neighbor_count;
uint8_t flash_counter;
uint8_t flood_counter;
uint8_t current_tick;

// Neighbor tracking
neighbor_info_t neighbors[MAX_NEIGHBORS];
uint8_t neighbor_count;

// Message buffer
message_t tx_msg;
uint8_t msg_sent = 0;

// Function declarations
void add_uid_to_known(uint16_t uid);
void update_neighbor(uint16_t uid, uint8_t nc);
void prune_old_neighbors(void);
uint8_t count_direct_neighbors(void);
void set_color_by_size(uint8_t size);
void check_if_line(void);

// Add UID to known set
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

// Update or add neighbor
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

// Remove old neighbors
void prune_old_neighbors(void) {
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

// Count direct neighbors
uint8_t count_direct_neighbors(void) {
    prune_old_neighbors();
    return neighbor_count;
}

// Set LED color
void set_color_by_size(uint8_t size) {
    switch (size) {
        case 1: set_color(RGB(3, 3, 3)); break;  // White
        case 2: set_color(RGB(3, 0, 0)); break;  // Red
        case 3: set_color(RGB(3, 3, 0)); break;  // Yellow
        case 4: set_color(RGB(0, 3, 0)); break;  // Green
        case 5: set_color(RGB(0, 0, 3)); break;  // Blue
        default: set_color(RGB(0, 0, 0)); break;
    }
}

// Check if line
void check_if_line(void) {
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

// Message RX - FIXED to properly decode messages
void message_rx(message_t *msg, distance_measurement_t *dist) {
    uint8_t i;
    uint8_t msg_type = msg->data[0];
    
    if (msg_type == MSG_UID_SET) {
        // Extract sender UID
        uint16_t sender_uid = (msg->data[1] << 8) | msg->data[2];
        uint8_t uid_count = msg->data[3];
        
        // Add sender's UID to known set
        add_uid_to_known(sender_uid);
        update_neighbor(sender_uid, 0);
        
        // Extract UIDs from message (up to 2 additional UIDs fit in 9 bytes)
        for (i = 0; i < uid_count && i < 2; i++) {
            uint16_t uid = (msg->data[4 + i*2] << 8) | msg->data[5 + i*2];
            if (uid != 0) {  // Valid UID
                add_uid_to_known(uid);
            }
        }
    } 
    else if (msg_type == MSG_NEIGHBOR_COUNT) {
        uint16_t sender_uid = (msg->data[1] << 8) | msg->data[2];
        uint8_t nc = msg->data[3];
        add_uid_to_known(sender_uid);
        update_neighbor(sender_uid, nc);
    }
}

// Message TX
message_t *message_tx(void) {
    return &tx_msg;
}

// Message TX success
void message_tx_success(void) {
    msg_sent = 1;
}

// Setup
void setup(void) {
    uint8_t i;
    
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
    msg_sent = 0;
    
    // Initialize known UIDs with own UID
    known_uids[0] = my_uid;
    known_uid_count = 1;
    prev_size = 1;
    
    // Clear message buffer
    for (i = 0; i < 9; i++) {
        tx_msg.data[i] = 0;
    }
    tx_msg.type = 0;
    tx_msg.crc = 0;
    
    set_color(RGB(3, 3, 3));
}

// Main loop
void loop(void) {
    uint8_t graph_size;
    uint8_t i;
    uint8_t uid_to_send;
    
    current_tick++;
    
    if (mode == GRAPH_MODE) {
        // Prepare UID set message
        tx_msg.data[0] = MSG_UID_SET;
        tx_msg.data[1] = (my_uid >> 8) & 0xFF;
        tx_msg.data[2] = my_uid & 0xFF;
        tx_msg.data[3] = known_uid_count;
        
        // Send up to 2 known UIDs (message limit is 9 bytes)
        uid_to_send = (known_uid_count < 2) ? known_uid_count : 2;
        for (i = 0; i < uid_to_send; i++) {
            tx_msg.data[4 + i*2] = (known_uids[i] >> 8) & 0xFF;
            tx_msg.data[5 + i*2] = known_uids[i] & 0xFF;
        }
        // Pad rest with zeros
        for (i = 4 + uid_to_send*2; i < 9; i++) {
            tx_msg.data[i] = 0;
        }
        
        tx_msg.type = NORMAL;
        tx_msg.crc = message_crc(&tx_msg);
        
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
    }
    else if (mode == LINE_CHECK) {
        my_neighbor_count = count_direct_neighbors();
        
        tx_msg.data[0] = MSG_NEIGHBOR_COUNT;
        tx_msg.data[1] = (my_uid >> 8) & 0xFF;
        tx_msg.data[2] = my_uid & 0xFF;
        tx_msg.data[3] = my_neighbor_count;
        for (i = 4; i < 9; i++) {
            tx_msg.data[i] = 0;
        }
        tx_msg.type = NORMAL;
        tx_msg.crc = message_crc(&tx_msg);
        
        flood_counter++;
        if (flood_counter >= FLOOD_CYCLES) {
            check_if_line();
            if (mode == LINE_CHECK) {
                mode = GRAPH_MODE;
            }
        }
    }
    else if (mode == LINE_MODE) {
        my_neighbor_count = count_direct_neighbors();
        
        tx_msg.data[0] = MSG_NEIGHBOR_COUNT;
        tx_msg.data[1] = (my_uid >> 8) & 0xFF;
        tx_msg.data[2] = my_uid & 0xFF;
        tx_msg.data[3] = my_neighbor_count;
        for (i = 4; i < 9; i++) {
            tx_msg.data[i] = 0;
        }
        tx_msg.type = NORMAL;
        tx_msg.crc = message_crc(&tx_msg);
        
        if (my_neighbor_count >= 3) {
            mode = GRAPH_MODE;
        }
        else if (my_neighbor_count <= 1) {
            if (known_uid_count == 1) {
                set_color(RGB(0, 0, 3));
                mode = ISOLATED;
            } else {
                mode = FLASHING;
                flash_counter = 0;
            }
        }
    }
    else if (mode == FLASHING) {
        if (flash_counter < FLASH_DURATION) {
            if ((flash_counter / 16) % 2 == 0) {
                set_color(RGB(3, 3, 3));
            } else {
                set_color(RGB(0, 0, 0));
            }
            flash_counter++;
        } else {
            mode = DRIVING_AWAY;
            set_motors(kilo_straight_left, kilo_straight_right);
        }
    }
    else if (mode == DRIVING_AWAY) {
        if (count_direct_neighbors() == 0) {
            set_motors(0, 0);
            known_uid_count = 1;
            known_uids[0] = my_uid;
            neighbor_count = 0;
            mode = GRAPH_MODE;
            stable_count = 0;
            prev_size = 1;
        }
    }
}

int main(void) {
    kilo_init();
    kilo_message_rx = message_rx;
    kilo_message_tx = message_tx;
    kilo_message_tx_success = message_tx_success;
    kilo_start(setup, loop);
    
    return 0;
}
