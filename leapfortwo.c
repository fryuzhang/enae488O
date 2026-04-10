#include "kilolib.h"

#define NUM_KILOBOTS 2
#define LEAP_COMPLETE_DISTANCE 40
#define RECENT_MESSAGE_TICKS 64

typedef enum { STOP, FORWARD, LEFT, RIGHT } motion_t;

static motion_t cur_motion = STOP;
static message_t msg;
static uint8_t new_message = 0;
static distance_measurement_t dist;

static uint16_t rx_sender_uid = 0;
static uint8_t rx_phase_id = 0;

static uint16_t leader_uid = 1001;
static uint16_t frog_uid   = 1002;
static uint8_t phase_id = 0;

static uint8_t distance_to_frog = 0;
static uint32_t last_frog_message_tick = 0;

static const uint16_t uid_ring[NUM_KILOBOTS] = {1001, 1002};

static inline void pack_u16(uint8_t *data, uint8_t idx, uint16_t value)
{
    data[idx] = value & 0xFF;
    data[idx+1] = (value >> 8);
}

static inline uint16_t unpack_u16(uint8_t *data, uint8_t idx)
{
    return data[idx] | (data[idx+1] << 8);
}

void set_motion(motion_t m)
{
    if (cur_motion != m) {
        cur_motion = m;
        switch(m) {
            case STOP: set_motors(0,0); break;
            case FORWARD: spinup_motors(); set_motors(kilo_straight_left,kilo_straight_right); break;
            case LEFT: spinup_motors(); set_motors(kilo_turn_left,0); break;
            case RIGHT: spinup_motors(); set_motors(0,kilo_turn_right); break;
        }
    }
}

void update_color()
{
    if (kilo_uid == leader_uid) set_color(RGB(0,1,0));
    else if (kilo_uid == frog_uid) set_color(RGB(1,0,0));
}

void update_roles()
{
    leader_uid = uid_ring[phase_id % NUM_KILOBOTS];
    frog_uid   = uid_ring[(phase_id + NUM_KILOBOTS - 1) % NUM_KILOBOTS];
}

void update_message()
{
    msg.type = NORMAL;
    pack_u16(msg.data,0,kilo_uid);
    msg.data[2] = phase_id;
    msg.crc = message_crc(&msg);
}

void setup()
{
    update_roles();
    update_color();
    update_message();
}

void loop()
{
    if (new_message) {
        new_message = 0;

        if (rx_phase_id > phase_id) {
            phase_id = rx_phase_id;
            update_roles();
        }

        if ((kilo_uid == leader_uid) && (rx_sender_uid == frog_uid)) {
            distance_to_frog = estimate_distance(&dist);
            last_frog_message_tick = kilo_ticks;

            if (distance_to_frog <= LEAP_COMPLETE_DISTANCE && distance_to_frog > 0) {
                phase_id++;
                update_roles();
                update_message();
                return;
            }
        }
    }

    update_color();

    if (kilo_uid == leader_uid) {
        if (distance_to_frog > LEAP_COMPLETE_DISTANCE)
            set_motion(FORWARD);
        else
            set_motion(STOP);
    } else {
        set_motion(STOP);
    }
}

message_t *message_tx() { return &msg; }

void message_rx(message_t *m, distance_measurement_t *d)
{
    new_message = 1;
    dist = *d;
    rx_sender_uid = unpack_u16(m->data,0);
    rx_phase_id = m->data[2];
}

int main()
{
    kilo_init();
    kilo_message_tx = message_tx;
    kilo_message_rx = message_rx;
    kilo_start(setup, loop);
    return 0;
}
