// This assumes `kilo_uid` values are `0, 1, 2, 3, 4`. 
#include "kilolib.h"
#include <stdint.h>
#include <math.h>

#define NUM_KILOBOTS 5
#define MAX_LOCAL_NEIGHBORS 16

#define STOP    0
#define FORWARD 1
#define LEFT    2
#define RIGHT   3

typedef uint8_t motion_t;

/* 
 * dist stores the raw distance measurement from a received message.
 * cur_distance stores the estimated distance from that measurement.
 */
distance_measurement_t dist;
uint8_t cur_distance = 0;

/*
 * msg is the message this robot broadcasts.
 * rx_kilo_id stores the received robot ID.
 * new_message is just a flag saying a message was received.
 */
message_t msg;
uint8_t rx_kilo_id;
uint8_t new_message = 0;

/*
 * local_network[] stores direct neighbors this robot has heard from.
 *
 * Important:
 * local_network[] uses 1-based storage so that 0 can mean "empty slot".
 *
 * Example:
 * If this robot hears kilo_uid = 2, then it stores 3.
 * That is why message_rx() uses:
 *
 * rx_kilo_id = m->data[0] + 1;
 *
 * So:
 * kilo_uid 0 is stored as 1
 * kilo_uid 1 is stored as 2
 * kilo_uid 2 is stored as 3
 * kilo_uid 3 is stored as 4
 * kilo_uid 4 is stored as 5
 */
uint16_t local_network[MAX_LOCAL_NEIGHBORS];

/*
 * heartbeat_check[] tracks how long it has been since each local neighbor
 * was last heard from.
 *
 * heartbeat_check[i] corresponds directly to local_network[i].
 */
uint16_t heartbeat_check[MAX_LOCAL_NEIGHBORS];

/*
 * num_neighbors is the number of direct neighbors currently stored
 * in local_network[].
 */
uint8_t num_neighbors = 0;

/*
 * local_network_binary[] is a bit-style representation of this robot's
 * local network.
 *
 * Example:
 * local_network_binary[0] = 1 means robot 0 is known.
 * local_network_binary[1] = 1 means robot 1 is known.
 * local_network_binary[2] = 1 means robot 2 is known.
 *
 * This eventually gets packed into one byte.
 */
uint8_t local_network_binary[8];

/*
 * send_local_network_binary is the packed binary version of local_network_binary[].
 *
 * Example:
 * If robots 0, 2, and 4 are known, the binary mask is:
 *
 * bit 4 bit 3 bit 2 bit 1 bit 0
 *   1     0     1     0     1
 *
 * That represents robots 0, 2, and 4.
 */
uint8_t send_local_network_binary = 0;

/*
 * neighbor_network_binaries[i] stores the binary local-network mask
 * associated with robot i.
 *
 * Example:
 * neighbor_network_binaries[2] stores what robot 2 knows about its local network.
 *
 * The global communication graph is estimated by OR-ing these masks together.
 */
uint8_t neighbor_network_binaries[NUM_KILOBOTS];

/*
 * global_cardinality is the number of robots in the global communication graph,
 * including this robot.
 */
uint8_t global_cardinality = 1;

/*
 * If a direct neighbor is not heard from for this many heartbeat updates,
 * it is removed from the local network.
 */
uint8_t IN_CONTACT_THRESHOLD = 4;

/*
 * start_time controls the heartbeat update interval.
 * global_timer controls how often we recompute and display the global graph size.
 */
uint32_t start_time;
uint32_t global_timer;

/* Current motor state. */
motion_t cur_motion = STOP;

void set_motion(motion_t new_motion)
{
    /*
     * Only update the motors when the desired motion changes.
     * This avoids repeatedly sending the same motor command.
     */
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

uint8_t count_bits(uint8_t value)
{
    /*
     * Counts how many robot bits are set to 1 in a binary graph masks
     */
    uint8_t count = 0;

    for (uint8_t i = 0; i < NUM_KILOBOTS; i++) {
        if ((value & (1 << i)) != 0) {
            count += 1;
        }
    }

    return count;
}

void set_color_based_on_cardinality(uint8_t cardinality)
{
    /*
     * Assignment color rule:
     *
     * 1 robot  including self -> white
     * 2 robots including self -> red
     * 3 robots including self -> yellow
     * 4 robots including self -> green
     * 5 robots including self -> blue
     */
    switch (cardinality)
    {
        case 1:
            set_color(RGB(1,1,1));   // white
            break;

        case 2:
            set_color(RGB(1,0,0));   // red
            break;

        case 3:
            set_color(RGB(1,1,0));   // yellow
            break;

        case 4:
            set_color(RGB(0,1,0));   // green
            break;

        case 5:
            set_color(RGB(0,0,1));   // blue
            break;

        default:
            set_color(RGB(0,0,0));   // off for unexpected values
            break;
    }
}

void check_heartbeats()
{
    /*
     * This function removes direct neighbors that have not been heard from
     * recently.
     *
     * Each neighbor has a heartbeat counter.
     * If the counter gets too large, the neighbor is assumed to have left
     * the local communication range.
     */
    uint8_t counter = 0;

    while (counter < num_neighbors)
    {
        if (heartbeat_check[counter] >= IN_CONTACT_THRESHOLD)
        {
            /*
             * local_network[counter] is stored as 1-5.
             * Convert it back to 0-4 by subtracting 1.
             */
            if (local_network[counter] != 0) {
                neighbor_network_binaries[local_network[counter] - 1] = 0;
            }

            /*
             * Remove the lost neighbor from local_network[] and heartbeat_check[].
             * If the lost neighbor is not at the end, shift everything left.
             */
            if (counter == num_neighbors - 1)
            {
                local_network[counter] = 0;
                heartbeat_check[counter] = 0;
            }
            else
            {
                for (uint8_t i = counter; i < num_neighbors - 1; i++)
                {
                    local_network[i] = local_network[i + 1];
                    heartbeat_check[i] = heartbeat_check[i + 1];
                }

                local_network[num_neighbors - 1] = 0;
                heartbeat_check[num_neighbors - 1] = 0;
            }

            num_neighbors -= 1;
        }
        else
        {
            counter += 1;
        }
    }
}

void update_local_network_binary()
{
    /*
     * This function converts local_network[] into a binary mask.
     *
     * Example:
     * If this robot knows robots 0, 2, and 4, then:
     *
     * local_network_binary[0] = 1
     * local_network_binary[2] = 1
     * local_network_binary[4] = 1
     *
     * Then this gets packed into send_local_network_binary.
     */

    send_local_network_binary = 0;

    /* Clear old binary values. */
    for (uint8_t i = 0; i < 8; i++)
    {
        local_network_binary[i] = 0;
    }

    /*
     * Mark every direct neighbor in the binary array.
     * local_network[i] is stored as 1-5, so subtract 1 to get 0-4.
     */
    for (uint8_t i = 0; i < num_neighbors; i++)
    {
        if (local_network[i] != 0)
        {
            local_network_binary[local_network[i] - 1] = 1;
        }
    }

    /*
     * A robot always knows itself, so it always marks its own bit as 1.
     */
    if (kilo_uid < NUM_KILOBOTS)
    {
        local_network_binary[kilo_uid] = 1;
    }

    /*
     * Pack the binary array into one byte.
     *
     * Example:
     * local_network_binary[0] = 1 becomes bit 0.
     * local_network_binary[1] = 1 becomes bit 1.
     */
    for (uint8_t i = 0; i < 8; i++)
    {
        send_local_network_binary |= (local_network_binary[i] << i);
    }

    /*
     * Store this robot's own local-network mask in the global array.
     */
    if (kilo_uid < NUM_KILOBOTS)
    {
        neighbor_network_binaries[kilo_uid] = send_local_network_binary;
    }
}

void update_global_cardinality()
{
    /*
     * The global communication graph is estimated by OR-ing together
     * the local-network masks from all robots.
     *
     * If any robot says robot X exists, then robot X is included in the
     * global network mask.
     */
    uint8_t global_network_binary = 0;

    for (uint8_t i = 0; i < NUM_KILOBOTS; i++)
    {
        global_network_binary |= neighbor_network_binaries[i];
    }

    /*
     * Count the number of 1 bits in the final combined graph mask.
     * This gives the global graph cardinality.
     */
    global_cardinality = count_bits(global_network_binary);

    /*
     * The graph should always include at least this robot.
     */
    if (global_cardinality < 1)
    {
        global_cardinality = 1;
    }
}

void setup()
{
    /*
     * Clear all neighbor and graph arrays at startup.
     */
    for (uint8_t i = 0; i < MAX_LOCAL_NEIGHBORS; i++)
    {
        local_network[i] = 0;
        heartbeat_check[i] = 0;
    }

    for (uint8_t i = 0; i < NUM_KILOBOTS; i++)
    {
        neighbor_network_binaries[i] = 0;
    }

    for (uint8_t i = 0; i < 8; i++)
    {
        local_network_binary[i] = 0;
    }

    /*
     * Initialize message data.
     * data[0] stores this robot's kilo_uid.
     * data[1] through data[5] store network binary values.
     */
    msg.type = NORMAL;
    msg.data[0] = kilo_uid;

    for (uint8_t i = 1; i < 9; i++)
    {
        msg.data[i] = 0;
    }

    msg.crc = message_crc(&msg);

    /*
     * Initialize timers.
     */
    start_time = kilo_ticks;
    global_timer = kilo_ticks;

    /*
     * Build this robot's first local-network mask.
     * At startup, this should mostly just include itself.
     */
    update_local_network_binary();
    update_global_cardinality();

    /*
     * Set initial LED color.
     */
    set_color_based_on_cardinality(global_cardinality);
}

void loop()
{
    /*
     * If a message was received, clear the flag.
     * The actual message processing already happened inside message_rx().
     */
    if (new_message == 1)
    {
        new_message = 0;
    }

    /*
     * Every 16 ticks, roughly half a second, increment each neighbor's
     * heartbeat counter.
     *
     * If a neighbor keeps sending messages, message_rx() resets its counter.
     */
    if ((kilo_ticks - start_time) >= 16)
    {
        for (uint8_t i = 0; i < num_neighbors; i++)
        {
            heartbeat_check[i] += 1;
        }

        start_time = kilo_ticks;
    }

    /*
     * Remove stale neighbors if they have not been heard from recently.
     */
    check_heartbeats();

    /*
     * Rebuild this robot's local-network binary mask after any heartbeat updates.
     */
    update_local_network_binary();

    /*
     * Every 3 seconds, recompute the global graph cardinality
     * and update the LED color.
     */
    if ((kilo_ticks - global_timer) >= (32 * 3))
    {
        update_global_cardinality();

        set_color_based_on_cardinality(global_cardinality);

        global_timer = kilo_ticks;
    }
}

message_t *message_tx()
{
    /*
     * This function is called automatically when the robot broadcasts.
     *
     * The message sends:
     * data[0] = this robot's kilo_uid
     * data[1] = network binary for robot 0
     * data[2] = network binary for robot 1
     * data[3] = network binary for robot 2
     * data[4] = network binary for robot 3
     * data[5] = network binary for robot 4
     */
    msg.type = NORMAL;

    msg.data[0] = (uint8_t) kilo_uid;

    for (uint8_t i = 0; i < NUM_KILOBOTS; i++)
    {
        msg.data[i + 1] = neighbor_network_binaries[i];
    }

    /*
     * Clear unused message bytes.
     */
    for (uint8_t i = NUM_KILOBOTS + 1; i < 9; i++)
    {
        msg.data[i] = 0;
    }

    msg.crc = message_crc(&msg);

    return &msg;
}

void message_rx(message_t *m, distance_measurement_t *d)
{
    /*
     * This function is called automatically when a message is received.
     */
    new_message = 1;

    /*
     * Estimate distance from the sender.
     * Distance is not required for graph cardinality, but it is kept from
     * the original structure.
     */
    dist = *d;
    cur_distance = estimate_distance(&dist);

    /*
     * Ignore messages from invalid UIDs.
     * Valid UIDs are 0, 1, 2, 3, 4.
     */
    if (m->data[0] >= NUM_KILOBOTS)
    {
        return;
    }

    /*
     * Store received UID using 1-based storage.
     * This allows 0 to mean "empty slot" in local_network[].
     */
    rx_kilo_id = m->data[0] + 1;

    /*
     * Copy the sender's advertised network binaries.
     * These represent what the sender knows about each robot's local network.
     */
    for (uint8_t i = 0; i < NUM_KILOBOTS; i++)
    {
        if (i != kilo_uid)
        {
            neighbor_network_binaries[i] = m->data[i + 1];
        }
    }

    /*
     * If this robot currently has no direct neighbors,
     * add the sender as the first direct neighbor.
     */
    if (num_neighbors == 0)
    {
        num_neighbors = 1;
        local_network[0] = rx_kilo_id;
        heartbeat_check[0] = 0;
    }
    else
    {
        uint8_t check_for_id = 0;
        uint8_t index = 0;

        /*
         * Check whether the sender is already stored in local_network[].
         */
        for (uint8_t i = 0; i < num_neighbors; i++)
        {
            if (rx_kilo_id == local_network[i])
            {
                check_for_id = 1;
                index = i;
            }
        }

        /*
         * If the sender is already known, reset its heartbeat counter.
         */
        if (check_for_id == 1)
        {
            heartbeat_check[index] = 0;
        }

        /*
         * If the sender is new, add it to the local neighbor list.
         */
        if (check_for_id == 0)
        {
            if (num_neighbors < MAX_LOCAL_NEIGHBORS)
            {
                local_network[num_neighbors] = rx_kilo_id;
                heartbeat_check[num_neighbors] = 0;
                num_neighbors += 1;
            }
        }

        /*
         * Double-check num_neighbors by counting nonzero entries
         * in local_network[].
         */
        uint8_t num_neighbors_check = 0;

        for (uint8_t i = 0; i < MAX_LOCAL_NEIGHBORS; i++)
        {
            if (local_network[i] != 0)
            {
                num_neighbors_check += 1;
            }
        }

        if (num_neighbors != num_neighbors_check)
        {
            num_neighbors = num_neighbors_check;
        }
    }

    /*
     * After receiving a message, immediately update the local and global graph.
     */
    update_local_network_binary();
    update_global_cardinality();
}

int main()
{
    kilo_init();

    /*
     * Register communication callbacks.
     */
    kilo_message_rx = message_rx;
    kilo_message_tx = message_tx;

    kilo_start(setup, loop);

    return 0;
}
```
