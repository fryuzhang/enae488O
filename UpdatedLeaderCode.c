#include "kilolib.h"
#define ELECTION_WINDOW 160
uint16_t my_rank     = 0xFFFF;
int      heard_lower = 0;
uint32_t start_ticks = 0;
uint8_t  message_sent = 0;
int       new_message = 0;
uint16_t  rcvd_rank   = 0xFFFF;
uint16_t  rcvd_uid    = 0xFFFF;

message_t tx_msg;
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

void message_tx_success() {
    message_sent = 1;
}

void message_rx(message_t *msg, distance_measurement_t *dist) {
    rcvd_rank   = msg->data[0] | (msg->data[1] << 8);
    rcvd_uid    = msg->data[2] | (msg->data[3] << 8);
    new_message = 1;
}

void show_rank_color() {
    if (my_rank == 0xFFFF) {
        set_color(RGB(0,0,1));
        return;
    }

    uint8_t tier  = my_rank / 6;
    uint8_t slot  = my_rank % 6;
    uint8_t slot1 = slot + 1;
    uint8_t r = (slot1 & 0x01) ? 1 : 0;
    uint8_t g = (slot1 & 0x02) ? 1 : 0;
    uint8_t b = (slot1 & 0x04) ? 1 : 0;

    uint16_t color_on  = 20;
    uint16_t color_off = 12;
    uint16_t white_on  = 16;
    uint16_t white_off = 12;
    uint16_t end_gap   = 28;

    uint16_t cycle_len = color_on + color_off + tier * (white_on + white_off)+ end_gap;

    uint16_t t = kilo_ticks % cycle_len;

    if (t < color_on) {
        set_color(RGB(r, g, b));
        return;
    }
    t -= color_on;
    if (t < color_off) {
        set_color(RGB(0,0,0));
        return;
    }
    t -= color_off;

    uint8_t i;
    for (i = 0; i < tier; i++) {
        if (t < white_on) {
            set_color(RGB(1,1,1));
            return;
        }
        t -= white_on;

        if (t < white_off) {
            set_color(RGB(0,0,0));
            return;
        }
        t -= white_off;
    }

    set_color(RGB(0,0,0));
}

void setup() {
    start_ticks = kilo_ticks;
}

void loop() {
    if (message_sent) {
        message_sent = 0;
        set_color(RGB(1,1,1));
        delay(10);
    }
    if (new_message) {
        new_message = 0;

        if (rcvd_uid < kilo_uid) {
            heard_lower = 1;
        }

        if (rcvd_rank != 0xFFFF) {
            uint16_t candidate = rcvd_rank + 1;
            if (candidate < my_rank) {
                my_rank = candidate;
                build_tx_msg();
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
    kilo_message_tx         = message_tx;
    kilo_message_tx_success = message_tx_success;
    kilo_message_rx         = message_rx;
    build_tx_msg();                    
    kilo_start(setup, loop);
    return 0;
}
