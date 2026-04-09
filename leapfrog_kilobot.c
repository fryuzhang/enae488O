#include "kilolib.h"

/* -------- USER SETTINGS -------- */
#define NUM_BOTS 4

/* Hardware UID order (physical lineup) */
unsigned int ORDER_UIDS[NUM_BOTS] = {1001, 1002, 1003, 1004};

/* Leap completion threshold */
#define LEAP_COMPLETE_DIST 100.0

/* -------- MESSAGE FORMAT -------- */
#define MSG_UID_L     0
#define MSG_UID_H     1
#define MSG_LEADER_L  2
#define MSG_LEADER_H  3
#define MSG_EPOCH     4
#define MSG_FLAGS     5

#define FLAG_LEAP_DONE 1

/* -------- GLOBAL VARIABLES -------- */
message_t msg;

unsigned int current_leader_uid;
unsigned char leap_epoch = 0;
unsigned char new_message = 0;

/* received data */
unsigned int rx_sender_uid;
unsigned int rx_leader_uid;
unsigned char rx_epoch;
unsigned char rx_flags;

/* distance measurement */
float rx_distance;

/* leader tracking */
float dist_to_last = 10000.0;
unsigned char saw_last_robot = 0;

/* local state */
unsigned char leap_done_local = 0;

/* -------- HELPER FUNCTIONS -------- */

int get_index(unsigned int uid)
{
    int i;
    for (i = 0; i < NUM_BOTS; i++) {
        if (ORDER_UIDS[i] == uid) return i;
    }
    return -1;
}

unsigned int get_next_leader(unsigned int uid)
{
    int idx = get_index(uid);

    if (idx < 0) return ORDER_UIDS[0];

    idx++;
    if (idx >= NUM_BOTS) idx = 0;

    return ORDER_UIDS[idx];
}

unsigned int get_last_uid()
{
    return ORDER_UIDS[NUM_BOTS - 1];
}

/* -------- MESSAGE PACKING -------- */

void update_message()
{
    msg.type = NORMAL;

    /* include own UID (NEW) */
    msg.data[MSG_UID_L] = kilo_uid & 0xFF;
    msg.data[MSG_UID_H] = (kilo_uid >> 8) & 0xFF;

    /* leader info */
    msg.data[MSG_LEADER_L] = current_leader_uid & 0xFF;
    msg.data[MSG_LEADER_H] = (current_leader_uid >> 8) & 0xFF;

    msg.data[MSG_EPOCH] = leap_epoch;

    msg.data[MSG_FLAGS] = 0;
    if (leap_done_local) {
        msg.data[MSG_FLAGS] |= FLAG_LEAP_DONE;
    }

    msg.crc = message_crc(&msg);
}

/* -------- SETUP -------- */

void setup()
{
    current_leader_uid = ORDER_UIDS[0];
    leap_epoch = 0;
    leap_done_local = 0;

    update_message();
}

/* -------- MAIN LOOP -------- */

void loop()
{
    int my_index;

    if (new_message) {

        /* gradient-style sync */
        if (rx_epoch > leap_epoch) {
            leap_epoch = rx_epoch;
            current_leader_uid = rx_leader_uid;
            leap_done_local = 0;
            update_message();
        }

        /* leader handoff */
        if ((kilo_uid == current_leader_uid) &&
            (rx_flags & FLAG_LEAP_DONE)) {

            current_leader_uid = get_next_leader(current_leader_uid);
            leap_epoch++;
            leap_done_local = 0;

            update_message();
        }

        new_message = 0;
    }

    my_index = get_index(kilo_uid);

    /* -------- LEAP COMPLETION LOGIC -------- */
    if (kilo_uid == current_leader_uid) {

        if (saw_last_robot && dist_to_last > LEAP_COMPLETE_DIST) {
            leap_done_local = 1;
            update_message();
        }
    }

    /* -------- COLOR LOGIC -------- */
    if (kilo_uid == current_leader_uid) {
        set_color(RGB(0,1,0));   // GREEN = leader
    }
    else if (my_index == NUM_BOTS - 1) {
        set_color(RGB(1,0,0));   // RED = last
    }
    else {
        set_color(RGB(0,0,1));   // BLUE = middle
    }
}

/* -------- MESSAGE RECEIVE -------- */

void message_rx(message_t *m, distance_measurement_t *d)
{
    new_message = 1;

    /* unpack sender UID (NEW) */
    rx_sender_uid = m->data[MSG_UID_L] |
                   (m->data[MSG_UID_H] << 8);

    rx_leader_uid = m->data[MSG_LEADER_L] |
                   (m->data[MSG_LEADER_H] << 8);

    rx_epoch = m->data[MSG_EPOCH];
    rx_flags = m->data[MSG_FLAGS];

    /* actual distance measurement */
    rx_distance = estimate_distance(d);

    /* leader tracks distance to last robot */
    if (kilo_uid == current_leader_uid) {

        if (rx_sender_uid == get_last_uid()) {
            dist_to_last = rx_distance;
            saw_last_robot = 1;
        }
    }
}

/* -------- TRANSMIT -------- */

message_t *message_tx()
{
    return &msg;
}

/* -------- MAIN -------- */

int main()
{
    kilo_init();

    kilo_message_rx = message_rx;
    kilo_message_tx = message_tx;

    kilo_start(setup, loop);

    return 0;
}
