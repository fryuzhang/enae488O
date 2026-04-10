#include "kilolib.h"

/* Total number of robots participating in the leapfrog cycle. */
#define NUM_KILOBOTS 4

/*
 * Distance threshold for declaring a successful leap.
 * The current frog changes roles once it measures the current leader
 * at or below this distance.
 */
#define LEAP_COMPLETE_DISTANCE 40

/*
 * Number of ticks for which a leader distance reading is considered recent.
 * If the frog has not heard from the leader recently, it just keeps moving.
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

/*
 * Fields unpacked from the most recently received message.
 * These are the sender's view of the current leapfrog state.
 */
static uint16_t rx_sender_uid = 0;
static uint16_t rx_frog_uid = 1001;
static uint16_t rx_leader_uid = 1004;
static uint8_t rx_phase_id = 0;

/*
 * This robot's current shared understanding of the leapfrog state.
 * Initially: frog = 1001, leader = 1004.
 */
static uint16_t frog_uid = 1001;
static uint16_t leader_uid = 1004;
static uint8_t phase_id = 0;

/*
 * Most recent estimated distance from the current frog to the current leader.
 * Only meaningful on the robot whose hardware UID matches frog_uid.
 */
static uint8_t distance_to_leader = 0;

/* Tick count when the frog most recently heard from the leader. */
static uint32_t last_leader_message_tick = 0;

/*
 * The next frog is chosen by moving to the next UID in this ring.
 */
static const uint16_t uid_ring[NUM_KILOBOTS] = {1001, 1002, 1003, 1004};

/*
 * Pack a 16-bit value into two consecutive bytes of the message data array.
 * This is used to send UIDs inside msg.data[].
 */
static inline void pack_u16(uint8_t *data, uint8_t idx, uint16_t value)
{
    data[idx] = (uint8_t)(value & 0xFF);
    data[idx + 1] = (uint8_t)((value >> 8) & 0xFF);
}

/*
 * Unpack a 16-bit value from two consecutive bytes of the message data array.
 * This is used to recover UIDs from received messages.
 */
static inline uint16_t unpack_u16(uint8_t *data, uint8_t idx)
{
    return (uint16_t)data[idx] | ((uint16_t)data[idx + 1] << 8);
}

/*
 * Apply a new motor command only if it is different from the current one.
 * This avoids repeatedly sending the same motor command every loop.
 */
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

/*
 * Update the LED color based on the robot's current role.
 * frog = green, leader = red, all others = blue.
 */
void update_color(void)
{
    if (kilo_uid == frog_uid) {
        set_color(RGB(0,1,0));
    } else if (kilo_uid == leader_uid) {
        set_color(RGB(1,0,0));
    } else {
        set_color(RGB(0,0,1));
    }
}

/*
 * Refresh the outgoing message so it contains this robot's current view of:
 * - sender UID
 * - current frog UID
 * - current leader UID
 * - current phase ID
 *
 * Every robot continuously transmits this message through message_tx().
 */
void update_message(void)
{
    msg.type = NORMAL;
    pack_u16(msg.data, 0, kilo_uid);
    pack_u16(msg.data, 2, frog_uid);
    pack_u16(msg.data, 4, leader_uid);
    msg.data[6] = phase_id;
    msg.data[7] = 0;
    msg.data[8] = 0;
    msg.crc = message_crc(&msg);
}

/*
 * Return the position of a UID in the leapfrog ring.
 * If the UID is not found, return 0 as a safe fallback.
 */
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

/*
 * Clear the frog's stored leader-distance information for the new phase.
 */
void reset_phase_measurement(void)
{
    distance_to_leader = 0;
    last_leader_message_tick = 0;
}

/*
 * If a received message contains a newer phase ID, adopt that newer shared state.
 * This lets all robots synchronize to the same frog/leader assignment.
 */
void adopt_new_phase_from_message(void)
{
    if (rx_phase_id > phase_id) {
        phase_id = rx_phase_id;
        frog_uid = rx_frog_uid;
        leader_uid = rx_leader_uid;
        reset_phase_measurement();
        set_motion(STOP);
        update_color();
        update_message();
    }
}

/*
 * Advance the leapfrog cycle.
 *
 * Current behavior:
 * - next robot in uid_ring becomes the new frog
 * - old frog becomes the new leader
 * - phase_id increments so all robots can recognize the update
 */
void advance_leapfrog_phase(void)
{
    uint8_t current_index = uid_index(frog_uid);
    uint8_t next_index = (current_index + 1) % NUM_KILOBOTS;
    uint16_t old_frog_uid = frog_uid;

    frog_uid = uid_ring[next_index];
    leader_uid = old_frog_uid;
    phase_id++;

    reset_phase_measurement();
    set_motion(STOP);
    update_color();
    update_message();
}

/* Initial setup: assign the starting color and build the first message. */
void setup(void)
{
    update_color();
    update_message();
}

void loop(void)
{
    if (new_message) {
        new_message = 0;  // reset flag after processing a received message

        adopt_new_phase_from_message();  // sync to newer phase if another robot advanced

        // only the frog (moving robot) checks distance to the leader
        if ((kilo_uid == frog_uid) && (rx_sender_uid == leader_uid)) {
            distance_to_leader = estimate_distance(&dist);  // compute distance to leader
            last_leader_message_tick = kilo_ticks;           // record time of measurement

            // if close enough to leader → leap complete → rotate roles
            if ((distance_to_leader > 0) && (distance_to_leader <= LEAP_COMPLETE_DISTANCE)) {
                advance_leapfrog_phase();
                return;
            }
        }
    }

    update_color();  // update LED color based on role

    // motion logic: only frog moves
    if (kilo_uid == frog_uid) {

        // if we recently heard from leader, use that distance info
        if ((last_leader_message_tick > 0) && ((kilo_ticks - last_leader_message_tick) < RECENT_MESSAGE_TICKS)) {
            
            // move forward if still far away
            if (distance_to_leader > LEAP_COMPLETE_DISTANCE) {
                set_motion(FORWARD);
            } else {
                set_motion(STOP);  // stop if already close
            }

        } else {
            // no recent distance info → keep moving forward blindly
            set_motion(FORWARD);
        }

    } else {
        // all non-frog robots stay still
        set_motion(STOP);
    }
}

message_t *message_tx(void)
{
    return &msg;  // continuously broadcast current state message
}

void message_rx(message_t *m, distance_measurement_t *d)
{
    new_message = 1;  // flag that a new message has arrived

    dist = *d;  // store raw distance measurement

    // unpack sender UID from message
    rx_sender_uid = unpack_u16(m->data, 0);

    // unpack frog UID from message
    rx_frog_uid = unpack_u16(m->data, 2);

    // unpack leader UID from message
    rx_leader_uid = unpack_u16(m->data, 4);

    // unpack phase ID (used for synchronization)
    rx_phase_id = m->data[6];
}

int main(void)
{
    kilo_init();  // initialize kilobot hardware

    kilo_message_tx = message_tx;  // register transmit callback
    kilo_message_rx = message_rx;  // register receive callback

    kilo_start(setup, loop);  // start main execution loop

    return 0;
}
