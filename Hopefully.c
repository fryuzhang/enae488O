#include <stdint.h>
#include "kilolib.h"

#define NUM_ROBOTS 5

#define TICKS_PER_SEC 32
#define RESET_TICKS (4 * TICKS_PER_SEC)

message_t message;

/*
 * local_neighbors[i] = directly heard robot i
 * global_neighbors[i] = robot i is known to be in the communication graph
 *
 * Since hardware IDs are 1-5:
 * kilo_uid = 1 uses index 0
 * kilo_uid = 2 uses index 1
 * kilo_uid = 3 uses index 2
 * kilo_uid = 4 uses index 3
 * kilo_uid = 5 uses index 4
 */
uint8_t local_neighbors[NUM_ROBOTS];
uint8_t global_neighbors[NUM_ROBOTS];

uint8_t global_neighbor_count = 1;
unsigned long last_reset_ticks = 0;

/* Convert hardware UID 1-5 into array index 0-4. */
uint8_t uid_to_index(uint8_t uid)
{
    return uid - 1;
}

/* Check that the UID is one of the expected hardware IDs. */
uint8_t valid_uid(uint8_t uid)
{
    if (uid >= 1 && uid <= NUM_ROBOTS) {
        return 1;
    }

    return 0;
}

/* Count how many robots are in the known global communication graph. */
uint8_t count_global_neighbors()
{
    uint8_t i;
    uint8_t count = 0;

    for (i = 0; i < NUM_ROBOTS; i++) {
        count += global_neighbors[i];
    }

    return count;
}

/* Reset the graph so this robot only knows itself again. */
void reset_neighbors()
{
    uint8_t i;
    uint8_t my_index;

    for (i = 0; i < NUM_ROBOTS; i++) {
        local_neighbors[i] = 0;
        global_neighbors[i] = 0;
    }

    if (valid_uid((uint8_t)kilo_uid)) {
        my_index = uid_to_index((uint8_t)kilo_uid);

        local_neighbors[my_index] = 1;
        global_neighbors[my_index] = 1;
    }
}

/* Update the outgoing message with this robot's current graph knowledge. */
void update_message()
{
    uint8_t i;

    message.type = NORMAL;

    /*
     * data[0] stores this robot's hardware UID.
     * data[1] through data[5] store the global neighbor array.
     */
    message.data[0] = (uint8_t)kilo_uid;

    for (i = 0; i < NUM_ROBOTS; i++) {
        message.data[i + 1] = global_neighbors[i];
    }

    /* Clear unused message bytes. */
    for (i = NUM_ROBOTS + 1; i < 9; i++) {
        message.data[i] = 0;
    }

    message.crc = message_crc(&message);
}

/* Set LED color based on the global graph cardinality. */
void set_color_from_count(uint8_t count)
{
    if (count == 1) {
        set_color(RGB(3,3,3));   // white
    }

    if (count == 2) {
        set_color(RGB(3,0,0));   // red
    }

    if (count == 3) {
        set_color(RGB(3,3,0));   // yellow
    }

    if (count == 4) {
        set_color(RGB(0,3,0));   // green
    }

    if (count == 5) {
        set_color(RGB(0,0,3));   // blue
    }
}

/* Transmit callback: send this robot's current known global graph. */
message_t *message_tx()
{
    update_message();
    return &message;
}

/* Receive callback: merge another robot's graph information into this robot's graph. */
void message_rx(message_t *m, distance_measurement_t *d)
{
    uint8_t i;
    uint8_t sender_uid;
    uint8_t sender_index;

    sender_uid = m->data[0];

    if (valid_uid(sender_uid)) {
        sender_index = uid_to_index(sender_uid);

        /*
         * If we directly receive a message from this robot,
         * then it is a local neighbor and also part of the global graph.
         */
        local_neighbors[sender_index] = 1;
        global_neighbors[sender_index] = 1;
    }

    /*
     * Merge the sender's global neighbor array into our global neighbor array.
     * If the sender knows a robot exists, we learn that robot exists too.
     */
    for (i = 0; i < NUM_ROBOTS; i++) {
        if (m->data[i + 1] == 1) {
            global_neighbors[i] = 1;
        }
    }

    update_message();
}

void setup()
{
    reset_neighbors();
    update_message();

    global_neighbor_count = count_global_neighbors();
    set_color_from_count(global_neighbor_count);

    last_reset_ticks = kilo_ticks;
}

void loop()
{
    /*
     * Periodically reset graph knowledge so the graph is dynamic.
     * This prevents a robot from remembering old neighbors forever.
     */
    if (kilo_ticks - last_reset_ticks >= RESET_TICKS) {
        reset_neighbors();
        update_message();
        last_reset_ticks = kilo_ticks;
    }

    global_neighbor_count = count_global_neighbors();

    set_color_from_count(global_neighbor_count);
}

int main()
{
    kilo_init();

    kilo_message_tx = message_tx;
    kilo_message_rx = message_rx;

    kilo_start(setup, loop);

    return 0;
}
