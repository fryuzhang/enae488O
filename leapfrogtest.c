#include "kilolib.h"

// This code will be combing the gradient, communication, and orbit labs into one code and modifying it to fulfill the objective of leapfrog. This is just a template and still needs more work. 



// declaring variables

// motion variables
typedef enum {
    STOP,
    FORWARD,
    LEFT,
    RIGHT
} motion_t;

// gradient variables
uint16_t gradient_value = UINT16_MAX;
uint16_t recvd_gradient = 0;
uint8_t new_message = 0;
message_t msg;

void message_rx(message_t *m, distance_measurement_t *d) {
    new_message = 1;
    // unpack two 8-bit integers into one 16-bit integer
    recvd_gradient = m->data[0]  | (m->data[1]<<8);
}

message_t *message_tx() {
    if (gradient_value != UINT16_MAX)
        return &msg;
    else
        return '\0';
}

void update_message() {
    // pack one 16-bit integer into two 8-bit integers
    msg.data[0] = gradient_value&0xFF;
    msg.data[1] = (gradient_value>>8)&0xFF;
    msg.crc = message_crc(&msg);
}

void loop() {
  if (new message) {
    if (gradient_value > recvd_gradient+1) {
        gradient_value = recvd_gradient+1;
        update_message();
      }
      new_message = 0;
      switch(gradient_value%3) {
            case 0:
                set_color(RGB(1,0,0));
                break;
            case 1:
                set_color(RGB(0,1,0));
                break;
            case 2:
                set_color(RGB(0,0,1));
                break;
        }
      }
}




}
