#include "kilolib.h"
uint16_t my_rank     = 0xFFFF;   
int      heard_lower = 0;        
uint32_t start_ticks = 0;      
uint32_t last_blink = 0;
uint8_t  blink_state = 0;  
#define  ELECTION_WINDOW  160    

message_t tx_msg;
message_t rx_msg;
int       new_message = 0;
uint16_t  rcvd_rank   = 0xFFFF;
uint16_t  rcvd_uid    = 0xFFFF;

void build_tx_msg() {
    tx_msg.type    = NORMAL;
    tx_msg.data[0] = my_rank & 0xFF;
    tx_msg.data[1] = (my_rank >> 8) & 0xFF;
    tx_msg.data[2] = kilo_uid & 0xFF;
    tx_msg.data[3] = (kilo_uid >> 8) & 0xFF;
    tx_msg.crc     = message_crc(&tx_msg);
}

message_t *message_tx() {
    return &tx_msg;
}

void message_rx(message_t *msg, distance_measurement_t *dist) {
    rcvd_rank    = msg->data[0] | (msg->data[1] << 8);
    rcvd_uid     = msg->data[2] | (msg->data[3] << 8);
    new_message  = 1;
}

void show_rank_color() {
    if (my_rank == 0xFFFF) {
        set_color(RGB(0,0,1));
        return;
    }
	
    uint8_t tier  = my_rank / 7;
    uint8_t slot  = my_rank % 7;
    uint8_t r = (slot & 0x01) ? 1 : 0;
    uint8_t g = (slot & 0x02) ? 1 : 0;
    uint8_t b = (slot & 0x04) ? 1 : 0;
	
    if (tier == 0) {
        set_color(RGB(r, g, b));
        return;
    }

    uint32_t cycle = kilo_ticks % 64;
    if (tier == 1) {
        if (cycle < 16) set_color(RGB(r, g, b));
        else            set_color(RGB(0, 0, 0));

    } else if (tier == 2) {
        if      (cycle < 12) set_color(RGB(r, g, b));
        else if (cycle < 22) set_color(RGB(0, 0, 0));
        else if (cycle < 34) set_color(RGB(r, g, b));
        else                 set_color(RGB(0, 0, 0));

    } else {
        if (cycle < 8) set_color(RGB(1, 1, 1));
        else           set_color(RGB(0, 0, 0));
    }
}
	

void setup() {
    start_ticks = kilo_ticks;
    build_tx_msg();
}

void loop() {
    if (new_message) {
        new_message = 0;
        if (rcvd_uid < kilo_uid) {
            heard_lower = 1;
        }
        if (rcvd_rank != 0xFFFF) {
            uint16_t candidate = rcvd_rank + 1;
            if (candidate < my_rank) {
                my_rank = candidate;
                build_tx_msg();   /* update broadcast immediately */
            }
        }
    }
    if (my_rank == 0xFFFF &&
        kilo_ticks > start_ticks + ELECTION_WINDOW &&
        heard_lower == 0)
    {
        my_rank = 0;
        build_tx_msg();
    }
    show_rank_color();
}

int main() {
    kilo_init();
    kilo_message_tx = message_tx;
    kilo_message_rx = message_rx;
    kilo_start(setup, loop);
    return 0;
}
