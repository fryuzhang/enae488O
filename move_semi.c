#include "kilolib.h"

#define N 2 // total number of kilobots
#define TOOCLOSE_DIST (40 + (N) * 10) // 40 + 10N (mm)
#define DESIRED_DIST (60 + (N) * 10) // 60 + 10N (mm)
#define ONE_SECOND 32 // 32 ticks

uint32_t orbit_time = 3 * ONE_SECOND + N * ONE_SECOND; // orbit for 3 seconds + N seconds -> will need to tune

typedef enum {
    STOP,
    FORWARD,
    LEFT,
    RIGHT
} motion_t;

typedef enum {
    ORBIT_TOOCLOSE,
    ORBIT_NORMAL,
} orbit_state_t;

// motion and messages
motion_t cur_motion = STOP;
orbit_state_t orbit_state = ORBIT_NORMAL;
uint8_t cur_distance = 0;
uint8_t new_message = 0;
distance_measurement_t dist;

// time and orbit
uint32_t start_time = 0;
uint8_t is_orbiting = 0; // orbiting flag
uint8_t orbit_finished = 0; // finish flag

void set_motion(motion_t new_motion){
    if(cur_motion != new_motion){
        cur_motion = new_motion;

        switch (cur_motion)
        {
        case STOP:
            set_motors(0, 0);
            set_color(RGB(1, 0, 0)); // red
            break;
        case FORWARD:
            spinup_motors();
            set_motors(kilo_straight_left, kilo_straight_right);
            set_color(RGB(0, 1, 1)); // cyan
            break;
        case LEFT:
            spinup_motors();
            set_motors(kilo_turn_left, 0);
            set_color(RGB(0, 0, 1)); // blue
            break;
        case RIGHT:
            spinup_motors();
            set_motors(0, kilo_turn_right);
            set_color(RGB(0, 1, 0)); // green
            break;        
        default:
            break;
        }
    }
}

void orbit_normal() {
    if (cur_distance < TOOCLOSE_DIST) {
        orbit_state = ORBIT_TOOCLOSE;
    } else {
        if (cur_distance < DESIRED_DIST)
            set_motion(LEFT);
        else
            set_motion(RIGHT);
    }
}

void orbit_tooclose() {
    if (cur_distance >= DESIRED_DIST)
        orbit_state = ORBIT_NORMAL;
    else
        set_motion(FORWARD);
}

void setup(){}

void loop(){
    if(orbit_finished) {
        // stop and end
        set_motion(STOP);
        return;
    }

    if(new_message) {
        new_message = 0;
        cur_distance = estimate_distance(&dist);
        
        //start timer at first measurement
        if(!is_orbiting){
            start_time = kilo_ticks;
            is_orbiting = 1;
        }

    } else if (cur_distance == 0){// skip state machine if no distance measurement available
        return;
    }

    // check orbit time
    if (is_orbiting && (kilo_ticks - start_time >= orbit_time)) {
        orbit_finished = 1;
        // stop and end
        set_motion(STOP);
        return;
    }


    // Orbit state machine
    switch(orbit_state) {
        case ORBIT_NORMAL:
            orbit_normal();
            break;
        case ORBIT_TOOCLOSE:
            orbit_tooclose();
            break;
    }

}

void message_rx(message_t *m, distance_measurement_t *d) {
    new_message = 1;
    dist = *d;
}

int main() {
    kilo_init();
    kilo_message_rx = message_rx;
    kilo_start(setup, loop);

    return 0;
}
