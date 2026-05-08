#include <stdint.h>
#include <stddef.h>
#include "kilolib.h"

#define ROBOT_TOTAL 5
#define RESET_TIME 128


// Robot knowledge storage
uint8_t connected_robots[ROBOT_TOTAL];
uint8_t direct_contacts[ROBOT_TOTAL];

uint8_t component_size = 1;


// Communication variables
message_t packet;

uint32_t last_refresh = 0;


// LED color array
uint8_t led_table[5] = {
    RGB(3,3,3), // 1 robot
    RGB(3,0,0), // 2 robots
    RGB(3,3,0), // 3 robots
    RGB(0,3,0), // 4 robots
    RGB(0,0,3)  // 5 robots
};


// Count active entries
uint8_t count_known_robots()
{
    uint8_t total = 0;

    uint8_t i;
    for(i = 0; i < ROBOT_TOTAL; i++)
    {
        total += connected_robots[i];
    }

    return total;
}


// Reset robot knowledge
void clear_neighbor_data()
{
    uint8_t i;

    for(i = 0; i < ROBOT_TOTAL; i++)
    {
        connected_robots[i] = 0;
        direct_contacts[i] = 0;
    }

   
    connected_robots[kilo_uid] = 1;
    direct_contacts[kilo_uid] = 1;
}


// Build outgoing message
void prepare_packet()
{
    uint8_t i;

    packet.type = NORMAL;

    // first byte = sender UID
    packet.data[0] = kilo_uid;

    // remaining bytes = known robots
    for(i = 0; i < ROBOT_TOTAL; i++)
    {
        packet.data[i + 1] = connected_robots[i];
    }

    // clear unused bytes
    for(i = ROBOT_TOTAL + 1; i < 9; i++)
    {
        packet.data[i] = 0;
    }

    packet.crc = message_crc(&packet);
}


// Set LED color
void update_led(uint8_t amount)
{
    if(amount >= 1 && amount <= 5)
    {
        set_color(led_table[amount - 1]);
    }
}


// Setup
void setup()
{
    clear_neighbor_data();
    prepare_packet();
}


// Transmit callback
message_t *tx_message()
{
    prepare_packet();
    return &packet;
}


// Receive callback
void rx_message(message_t *msg, distance_measurement_t *dist)
{
    uint8_t sender = msg->data[0];

    // mark sender as direct neighbor
    direct_contacts[sender] = 1;

    // sender is also part of connected graph
    connected_robots[sender] = 1;

    // merge sender knowledge with local knowledge
    uint8_t i;

    for(i = 0; i < ROBOT_TOTAL; i++)
    {
        if(msg->data[i + 1] == 1)
        {
            connected_robots[i] = 1;
        }
    }
}


// Main loop
void loop()
{
    // periodic refresh to detect removals
    if(kilo_ticks - last_refresh > RESET_TIME)
    {
        clear_neighbor_data();
        last_refresh = kilo_ticks;
    }

    // compute connected component size
    component_size = count_known_robots();

    // update LED
    update_led(component_size);
}


// Main
int main()
{
    kilo_init();

    kilo_message_tx = tx_message;
    kilo_message_rx = rx_message;

    kilo_start(setup, loop);

    return 0;
}