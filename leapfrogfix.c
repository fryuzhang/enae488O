#include "kilolib.h"

/* Total number of robots participating in the leapfrog cycle. */
#define NUM_KILOBOTS 4

/*
 * Distance threshold for declaring a successful leap.
 * The current leader changes roles once it measures the current frog
 * at or below this distance.
 */
#define LEAP_COMPLETE_DISTANCE 40

/*
 * Number of ticks for which a frog distance reading is considered recent.
 * If the leader has not heard from the frog recently, it just keeps moving.
 */
#define RECENT_MESSAGE_TICKS 64

/* Motion states used by set_motion(). */
typedef enum {
    STOP,
    FORWARD,
    LEFT,
    RIGHT
} motion_t;

/* Current motion command being applied to this robot. */
static motion_t cur_motion = STOP;

/* Outgoing message buffer. Every robot continuously broadcasts this message. */
static message_t msg;

/*
 * new_message is set to 1 inside message_rx() whenever a message arrives.
 * The loop() function then processes that new message exactly once.
 */
static uint8_t new_message = 0;

/* Most recent raw distance measurement received with a message. */
static distance_measurement_t dist;

/* Fields unpacked from the most recently received message. */
static uint16_t rx_sender_uid = 0;
static uint8_t rx_phase_id = 0;

/* Shared leapfrog state. */
static uint8_t phase_id = 0;
static uint16_t leader_uid = 1001;
static uint16_t frog_uid = 1004;

/*
 * Most recent estimated distance from the current leader to the current frog.
 * Only meaningful on the robot whose hardware UID matches leader_uid.
 */
static uint8_t distance_to_frog = 0;

/* Tick count when the leader most recently heard from the frog. */
static uint32_t last_frog_message_tick = 0;

/* Fixed ordering of robots in the leapfrog cycle. */
static const uint16_t uid_ring[NUM_KILOBOTS] = {1001, 1002, 1003, 1004};

/* Pack a 16-bit value into two consecutive bytes of the message data array. */
static inline void pack_u16(uint8_t *data, uint8_t idx, uint16_t value)
{
    data[idx] = (uint8_t)(value & 0xFF);
    data[idx + 1] = (uint8_t)((value >> 8) & 0xFF);
}

/* Unpack a 16-bit value from two consecutive bytes of the message data array. */
static inline uint16_t unpack_u16(uint8_t *data, uint8_t idx)
{
    return (uint16_t)data[idx] | ((uint16_t)data[idx + 1] << 8);
}

/* Apply a new motor command only if it is different from the current one. */
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

/* Derive leader and frog directly from phase_id. */
void derive_roles_from_phase(void)
{
    leader_uid = uid_ring[phase_id % NUM_KILOBOTS];
    frog_uid = uid_ring[(phase_id + NUM_KILOBOTS - 1) % NUM_KILOBOTS];
}

/* Update the LED color based on the robot's current role. */
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

/* Refresh the outgoing message. */
void update_message(void)
{
    msg.type = NORMAL;
    pack_u16(msg.data, 0, kilo_uid);
    msg.data[2] = phase_id;
    msg.data[3] = 0;
    msg.data[4] = 0;
    msg.data[5] = 0;
    msg.data[6] = 0;
    msg.data[7] = 0;
    msg.data[8] = 0;
    msg.crc = message_crc(&msg);
}

/* Clear the leader's stored frog-distance information for the new phase. */
void reset_phase_measurement(void)
{
    distance_to_frog = 0;
    last_frog_message_tick = 0;
}

/* If a received message contains a newer phase ID, adopt that newer phase only. */
void adopt_new_phase_from_message(void)
{
    if (rx_phase_id > phase_id) {
        phase_id = rx_phase_id;
        derive_roles_from_phase();
        reset_phase_measurement();
        set_motion(STOP);
        update_color();
        update_message();
    }
}

/* Advance the leapfrog cycle by incrementing phase_id only. */
void advance_leapfrog_phase(void)
{
    phase_id++;
    derive_roles_from_phase();
    reset_phase_measurement();
    set_motion(STOP);
    update_color();
    update_message();
}

/* Initial setup. */
void setup(void)
{
    phase_id = 0;
    derive_roles_from_phase();
    reset_phase_measurement();
    update_color();
    update_message();
}

/* Main control loop. */
void loop(void)
{
    if (new_message) {
        new_message = 0;

        /* Synchronize to a newer phase if another robot already advanced it. */
        adopt_new_phase_from_message();

        /*
         * Only the current leader cares about distance to the current frog.
         * The sender UID is read from the received message payload.
         */
        if ((kilo_uid == leader_uid) && (rx_sender_uid == frog_uid)) {
            distance_to_frog = estimate_distance(&dist);
            last_frog_message_tick = kilo_ticks;

            /*
             * Successful leap condition:
             * if the leader is close enough to the frog, rotate the roles.
             */
            if ((distance_to_frog > 0) && (distance_to_frog <= LEAP_COMPLETE_DISTANCE)) {
                advance_leapfrog_phase();
                return;
            }
        }
    }

    /* Keep LED color consistent with the latest known role. */
    update_color();

    if (kilo_uid == leader_uid) {
        /*
         * If the leader has heard from the frog recently, use the measured distance.
         * Move forward while still too far away; otherwise stop.
         */
        if ((last_frog_message_tick > 0) && ((kilo_ticks - last_frog_message_tick) < RECENT_MESSAGE_TICKS)) {
            if (distance_to_frog > LEAP_COMPLETE_DISTANCE) {
                set_motion(FORWARD);
            } else {
                set_motion(STOP);
            }
        } else {
            /*
             * If the leader has not heard from the frog recently,
             * keep moving forward instead of waiting.
             */
            set_motion(FORWARD);
        }
    } else {
        /* Non-leaders stay still in this version. */
        set_motion(STOP);
    }
}

/* Continuously broadcast this robot's current message. */
message_t *message_tx(void)
{
    return &msg;
}

/* Receive callback. */
void message_rx(message_t *m, distance_measurement_t *d)
{
    new_message = 1;
    dist = *d;
    rx_sender_uid = unpack_u16(m->data, 0);
    rx_phase_id = m->data[2];
}

int main(void)
{
    kilo_init();
    kilo_message_tx = message_tx;
    kilo_message_rx = message_rx;
    kilo_start(setup, loop);
    return 0;
}
