#include <kilolib.h>

#define NUM_KILOBOTS 4

typedef enum {
    STOP,
    FORWARD,
    LEFT,
    RIGHT
} motion_t;

static motion_t cur_motion = STOP;

static message_t msg;
static uint8_t new_message = 0;
static distance_measurement_t dist;

static uint16_t rx_sender_uid = 0;
static uint16_t rx_leader_uid = 1001;
static uint16_t rx_frog_uid = 1004;
static uint8_t rx_phase_id = 0;

static uint16_t leader_uid = 1001;
static uint16_t frog_uid = 1004;
static uint8_t phase_id = 0;

static uint8_t distance_to_frog = 0;
static uint8_t phase_start_distance = 0;
static uint8_t phase_distance_set = 0;

static const uint16_t uid_ring[NUM_KILOBOTS] = {
    1001, 1002, 1003, 1004
};

static inline void pack_u16(uint8_t *data, uint8_t idx, uint16_t value)
{
    data[idx] = (uint8_t)(value & 0xFF);
    data[idx + 1] = (uint8_t)((value >> 8) & 0xFF);
}

static inline uint16_t unpack_u16(uint8_t *data, uint8_t idx)
{
    return (uint16_t)data[idx] | ((uint16_t)data[idx + 1] << 8);
}

void set_motion(motion_t new_motion)
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

void update_color(void)
{
    if (kilo_uid == leader_uid) {
        set_color(RGB(0,1,0));
    } else if (kilo_uid == frog_uid) {
        set_color(RGB(1,0,0));
    } else {
        set_color(RGB(0,0,1));
    }
}

void update_message(void)
{
    msg.type = NORMAL;

    pack_u16(msg.data, 0, kilo_uid);
    pack_u16(msg.data, 2, leader_uid);
    pack_u16(msg.data, 4, frog_uid);
    msg.data[6] = phase_id;
    msg.data[7] = 0;
    msg.data[8] = 0;

    msg.crc = message_crc(&msg);
}

uint8_t uid_index(uint16_t uid)
{
    uint8_t i;
    for (i = 0; i < NUM_KILOBOTS; i++) {
        if (uid_ring[i] == uid) {
            return i;
        }
    }
    return 0;
}

void reset_phase_measurement(void)
{
    distance_to_frog = 0;
    phase_start_distance = 0;
    phase_distance_set = 0;
}

void adopt_new_phase_from_message(void)
{
    if (rx_phase_id > phase_id) {
        phase_id = rx_phase_id;
        leader_uid = rx_leader_uid;
        frog_uid = rx_frog_uid;
        reset_phase_measurement();
        set_motion(STOP);
        update_color();
        update_message();
    }
}

void advance_leapfrog_phase(void)
{
    uint8_t current_index = uid_index(leader_uid);
    uint8_t next_index = (current_index + 1) % NUM_KILOBOTS;
    uint16_t old_leader_uid = leader_uid;

    leader_uid = uid_ring[next_index];
    frog_uid = old_leader_uid;
    phase_id++;

    reset_phase_measurement();
    set_motion(STOP);
    update_color();
    update_message();
}

void setup(void)
{
    update_color();
    update_message();
}

void loop(void)
{
    uint8_t threshold;

    if (new_message) {
        new_message = 0;

        adopt_new_phase_from_message();

        if ((kilo_uid == leader_uid) && (rx_sender_uid == frog_uid)) {
            distance_to_frog = estimate_distance(&dist);

            if ((phase_distance_set == 0) && (distance_to_frog > 0)) {
                phase_start_distance = distance_to_frog;
                phase_distance_set = 1;
            }
        }
    }

    update_color();

    if (kilo_uid == leader_uid) {
        if ((phase_distance_set == 0) || (distance_to_frog == 0)) {
            set_motion(STOP);
            return;
        }

        threshold = phase_start_distance / NUM_KILOBOTS;
        if (threshold == 0) {
            threshold = 1;
        }

        if (distance_to_frog <= threshold) {
            advance_leapfrog_phase();
        } else {
            set_motion(FORWARD);
        }
    } else {
        set_motion(STOP);
    }
}

message_t *message_tx(void)
{
    return &msg;
}

void message_rx(message_t *m, distance_measurement_t *d)
{
    new_message = 1;
    dist = *d;

    rx_sender_uid = unpack_u16(m->data, 0);
    rx_leader_uid = unpack_u16(m->data, 2);
    rx_frog_uid = unpack_u16(m->data, 4);
    rx_phase_id = m->data[6];
}

int main(void)
{
    kilo_init();
    kilo_message_tx = message_tx;
    kilo_message_rx = message_rx;
    kilo_start(setup, loop);
    return 0;
}
