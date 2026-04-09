#include "kilolib.h"

#define ELECTION_WINDOW  160
#define LEAP_DRIVE_TIME  200   // ticks driving forward (~6 sec)
#define LEAP_READY_WAIT  64    // ticks to wait for all bots to signal ready

// ── phases ──────────────────────────────────────────────────────────────────
#define PHASE_RANK      0
#define PHASE_LEAP_WAIT 1
#define PHASE_LEAPING   2
#define PHASE_REST      3
#define PHASE_DONE      4

// ── message types ────────────────────────────────────────────────────────────
#define MSG_RANK  0   // normal rank broadcast
#define MSG_READY 1   // "I'm ranked and ready to leap"
#define MSG_GO    2   // "start leaping now" (sent by rank-0 leader)

uint8_t  phase        = PHASE_RANK;
uint8_t  leap_count   = 0;          // how many leaps completed (max 2)
uint16_t my_rank      = 0xFFFF;
uint8_t  max_rank     = 0;          // highest rank seen (= N-1 if N bots)
uint8_t  ready_count  = 0;          // how many READY msgs heard this round
int      heard_lower  = 0;
uint32_t phase_start  = 0;
int      new_message  = 0;
uint16_t rcvd_rank    = 0xFFFF;
uint16_t rcvd_uid     = 0xFFFF;
uint8_t  rcvd_type    = MSG_RANK;

message_t tx_msg;

// ── helpers ──────────────────────────────────────────────────────────────────
void build_tx_msg_rank() {
    tx_msg.type    = NORMAL;
    tx_msg.data[0] = MSG_RANK;
    tx_msg.data[1] = my_rank & 0xFF;
    tx_msg.data[2] = (my_rank >> 8) & 0xFF;
    tx_msg.data[3] = kilo_uid & 0xFF;
    tx_msg.data[4] = (kilo_uid >> 8) & 0xFF;
    tx_msg.crc     = message_crc(&tx_msg);
}

void build_tx_msg_ready() {
    tx_msg.type    = NORMAL;
    tx_msg.data[0] = MSG_READY;
    tx_msg.data[1] = my_rank & 0xFF;
    tx_msg.data[2] = (my_rank >> 8) & 0xFF;
    tx_msg.crc     = message_crc(&tx_msg);
}

void build_tx_msg_go() {
    tx_msg.type    = NORMAL;
    tx_msg.data[0] = MSG_GO;
    tx_msg.crc     = message_crc(&tx_msg);
}

void spinup_and_drive() {
    set_motors(255, 255);
    delay(15);
    set_motors(kilo_straight_left, kilo_straight_right);
}

void stop_motors() {
    set_motors(0, 0);
}

void enter_phase(uint8_t p) {
    phase       = p;
    phase_start = kilo_ticks;
}

// ── callbacks ────────────────────────────────────────────────────────────────
message_t *message_tx() { return &tx_msg; }
void message_tx_success() {}

void message_rx(message_t *msg, distance_measurement_t *dist) {
    rcvd_type = msg->data[0];
    if (rcvd_type == MSG_RANK) {
        rcvd_rank = msg->data[1] | (msg->data[2] << 8);
        rcvd_uid  = msg->data[3] | (msg->data[4] << 8);
    } else if (rcvd_type == MSG_READY) {
        rcvd_rank = msg->data[1] | (msg->data[2] << 8);
    }
    new_message = 1;
}

// ── color display ─────────────────────────────────────────────────────────────
void show_rank_color() {
    if (my_rank == 0xFFFF) { set_color(RGB(0,0,1)); return; }
    uint8_t slot  = (my_rank % 6) + 1;
    uint8_t tier  = my_rank / 6;
    uint8_t r = (slot & 1) ? 1 : 0;
    uint8_t g = (slot & 2) ? 1 : 0;
    uint8_t b = (slot & 4) ? 1 : 0;
    uint16_t color_on=20, color_off=12, white_on=16, white_off=12, end_gap=28;
    uint16_t cycle_len = color_on + color_off + tier*(white_on+white_off) + end_gap;
    uint16_t t = kilo_ticks % cycle_len;
    if (t < color_on)  { set_color(RGB(r,g,b)); return; } t -= color_on;
    if (t < color_off) { set_color(RGB(0,0,0)); return; } t -= color_off;
    uint8_t i;
    for (i = 0; i < tier; i++) {
        if (t < white_on)  { set_color(RGB(1,1,1)); return; } t -= white_on;
        if (t < white_off) { set_color(RGB(0,0,0)); return; } t -= white_off;
    }
    set_color(RGB(0,0,0));
}

// ── setup & loop ─────────────────────────────────────────────────────────────
void setup() {
    build_tx_msg_rank();
    phase_start = kilo_ticks;
}

void loop() {

    // ── PHASE_RANK: distributed election (your original logic) ───────────────
    if (phase == PHASE_RANK) {
        if (new_message && rcvd_type == MSG_RANK) {
            new_message = 0;
            if (rcvd_uid < kilo_uid) heard_lower = 1;
            if (rcvd_rank > max_rank) max_rank = (uint8_t)rcvd_rank;
            if (rcvd_rank != 0xFFFF) {
                uint16_t candidate = rcvd_rank + 1;
                if (candidate < my_rank) {
                    my_rank = candidate;
                    build_tx_msg_rank();
                }
            }
        }
        // become rank 0 if no lower UID heard after window
        if (my_rank == 0xFFFF &&
            kilo_ticks > phase_start + ELECTION_WINDOW &&
            heard_lower == 0)
        {
            my_rank = 0;
            build_tx_msg_rank();
        }
        // once we have a rank, move to leap-wait
        if (my_rank != 0xFFFF &&
            kilo_ticks > phase_start + ELECTION_WINDOW + 32)
        {
            ready_count = 0;
            build_tx_msg_ready();
            enter_phase(PHASE_LEAP_WAIT);
        }
        show_rank_color();
        return;
    }

    // ── PHASE_LEAP_WAIT: count ready neighbours, wait for leader's GO ────────
    if (phase == PHASE_LEAP_WAIT) {
        if (new_message) {
            new_message = 0;
            if (rcvd_type == MSG_READY) {
                ready_count++;
                if (rcvd_rank > max_rank) max_rank = (uint8_t)rcvd_rank;
            }
            if (rcvd_type == MSG_GO) {
                // everyone got the GO — move to leaping phase
                enter_phase(PHASE_LEAPING);
                return;
            }
        }
        // rank-0 leader fires GO once it has heard all others ready
        if (my_rank == 0 && ready_count >= max_rank) {
            build_tx_msg_go();
            enter_phase(PHASE_LEAPING);
            return;
        }
        // pulse white while waiting
        set_color((kilo_ticks % 32) < 16 ? RGB(1,1,1) : RGB(0,0,0));
        return;
    }

    // ── PHASE_LEAPING: the tail bot (highest rank) drives forward ─────────────
    if (phase == PHASE_LEAPING) {
        uint8_t i_am_tail = (my_rank == max_rank);

        if (i_am_tail) {
            // drive straight for LEAP_DRIVE_TIME ticks
            if (kilo_ticks < phase_start + LEAP_DRIVE_TIME) {
                spinup_and_drive();
                set_color(RGB(0,3,0));   // bright green while moving
            } else {
                stop_motors();
                leap_count++;
                if (leap_count >= 2) {
                    enter_phase(PHASE_DONE);
                } else {
                    // reset ranks for next round
                    my_rank     = 0xFFFF;
                    max_rank    = 0;
                    heard_lower = 0;
                    ready_count = 0;
                    build_tx_msg_rank();
                    enter_phase(PHASE_RANK);
                }
            }
        } else {
            // everyone else stays still, pulses their rank color
            stop_motors();
            show_rank_color();
            // after generous timeout, assume leap done and re-enter ranking
            if (kilo_ticks > phase_start + LEAP_DRIVE_TIME + 64) {
                my_rank     = 0xFFFF;
                max_rank    = 0;
                heard_lower = 0;
                ready_count = 0;
                build_tx_msg_rank();
                enter_phase(PHASE_RANK);
            }
        }
        return;
    }

    // ── PHASE_REST: brief pause then jump back into ranking ───────────────────
    if (phase == PHASE_REST) {
        stop_motors();
        set_color(RGB(0,0,0));
        if (kilo_ticks > phase_start + LEAP_READY_WAIT) {
            enter_phase(PHASE_RANK);
        }
        return;
    }

    // ── PHASE_DONE ────────────────────────────────────────────────────────────
    if (phase == PHASE_DONE) {
        stop_motors();
        // slow red pulse = finished
        set_color((kilo_ticks % 64) < 32 ? RGB(1,0,0) : RGB(0,0,0));
        return;
    }
}

int main() {
    kilo_init();
    kilo_message_tx         = message_tx;
    kilo_message_tx_success = message_tx_success;
    kilo_message_rx         = message_rx;
    kilo_start(setup, loop);
    return 0;
}
