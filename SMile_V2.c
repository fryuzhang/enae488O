#include "kilobot.h"

// order set up is (555,111,444,333,222,666)

// Motion constants
#define STOP    0
#define FORWARD 1
#define LEFT    2
#define RIGHT   3

int cur_motion = STOP;

void set_motion(int new_motion) {
    cur_motion = new_motion;
    if (new_motion == STOP) {
        set_motors(0, 0);
    } else if (new_motion == FORWARD) {
        spinup_motors();
        set_motors(kilo_straight_left, kilo_straight_right);
    } else if (new_motion == LEFT) {
        spinup_motors();
        set_motors(kilo_turn_left, 0);
    } else if (new_motion == RIGHT) {
        spinup_motors();
        set_motors(0, kilo_turn_right);
    }
}

void setup() {
    if (kilo_uid == 111) {
        set_color(RGB(0, 0, 3));      // Eye - BLUE
    } else if (kilo_uid == 222) {
        set_color(RGB(0, 0, 3));      // Eye - BLUE
    } else if (kilo_uid == 333) {
        set_color(RGB(3, 0, 3));      // Nose - MAGENTA
    } else if (kilo_uid == 444) {
        set_color(RGB(3, 0, 0));      // Mouth - RED
    } else if (kilo_uid == 555) {
        set_color(RGB(3, 0, 0));      // Mouth - RED
    } else if (kilo_uid == 666) {
        set_color(RGB(3, 0, 0));      // Mouth - RED
    } else {
        set_color(RGB(1, 1, 1));      // Unknown - WHITE
    }
}

void movement() {

    if (kilo_uid == 111) {
        set_motion(FORWARD);
        delay(1200);                  // 3 chunks forward
        set_motion(STOP);
    }

    else if (kilo_uid == 222) {
        set_motion(FORWARD);
        delay(1200);                  // 3 chunks forward
        set_motion(STOP);
    }

    else if (kilo_uid == 333) {
        set_motion(FORWARD);
        delay(800);                   // 2 chunks forward
        set_motion(LEFT);
        delay(150);                   // slight left curve 
        set_motion(STOP);
    }

    else if (kilo_uid == 444) {
        set_motion(RIGHT);
        delay(600);                   // 180 degree turn
        set_motion(FORWARD);
        delay(400);                   // 1 chunk 
        set_motion(LEFT);
        delay(150);                   // slight right
        set_motion(STOP);
    }

   
    else if (kilo_uid == 555) {
        set_motion(STOP);
        delay(500);
        set_color(RGB(1, 1, 1));
        delay(500);
        set_color(RGB(3, 0, 0));
    }

    else if (kilo_uid == 666) {
        set_motion(STOP);
        delay(500);
        set_color(RGB(1, 1, 1));
        delay(500);
        set_color(RGB(3, 0, 0));
    }
}

void loop() {
    movement();
}

int main() {
    kilo_init();
    kilo_start(setup, loop);
    return 0;
}
