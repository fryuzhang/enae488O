#include "kilolib.h"

#define STOP    0
#define FORWARD 1
#define LEFT    2
#define RIGHT   3

uint8_t cur_motion = STOP;

/* Tracks whether this Kilobot has already completed its movement path. */
uint8_t movement_done = 0;

void set_motion(uint8_t new_motion)
{
    /* Only update the motors if the motion command has changed. */
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

void setup()
{
    /* At startup, no Kilobot has completed its motion yet. */
    movement_done = 0;

    if (kilo_uid == 1) {
        set_color(RGB(1,0,0));   // red: mouth robot
    }

    if (kilo_uid == 2) {
        set_color(RGB(0,0,1));   // blue: left eye robot, does not move
    }

    if (kilo_uid == 3) {
        set_color(RGB(1,0,0));   // red: mouth robot
    }

    if (kilo_uid == 4) {
        set_color(RGB(1,0,1));   // purple: nose robot
    }

    if (kilo_uid == 5) {
        set_color(RGB(0,0,1));   // blue: right eye robot, does not move
    }

    if (kilo_uid == 6) {
        set_color(RGB(1,0,0));   // red: mouth robot
    }
}

void loop()
{
    /* Run the movement sequence only once. */
    if (movement_done == 0)
    {
        /*
         * Starting line:
         * 1 2 3 4 5 6
         *
         * Robots 2 and 5 stay still as the eyes.
         * Robots 1, 3, and 6 move to form the mouth.
         * Robot 4 moves to form the nose.
         */

        if (kilo_uid == 1)
        {
            /* Robot 1: left side of mouth. */
            set_motion(FORWARD);
            delay(8000);
            
            
            set_motion(STOP);
            movement_done = 1;
            set_color(RGB(1,1,1));   // white: movement finished
            return;
        }

        if (kilo_uid == 2)
        {
            /* Robot 2: left eye, does not move. */
            set_motion(STOP);
            movement_done = 1;
            set_color(RGB(0,0,1));   // blue: left eye final color
            return;
        }

        if (kilo_uid == 3)
        {
            /* Robot 3: middle of mouth. */
            set_motion(FORWARD);
            delay(10000);
            
            set_motion(RIGHT);
            delay(750);
            
            set_motion(FORWARD);
            delay(5000);
          
            
            set_motion(STOP);
            movement_done = 1;
            set_color(RGB(1,1,1));   // white: movement finished
            return;
        }

        if (kilo_uid == 4)
        {
            /* Robot 4: nose. */
            delay(5000);
            
            set_motion(FORWARD);
            delay(9000);
          
            set_motion(STOP);
            movement_done = 1;
            set_color(RGB(1,0,1));   // purple: nose final color
            return;
        }

        if (kilo_uid == 5)
        {
            /* Robot 5: right eye, does not move. */
            set_motion(STOP);
            movement_done = 1;
            set_color(RGB(0,0,1));   // blue: right eye final color
            return;
        }

        if (kilo_uid == 6)
        {
            /* Robot 6: right side of mouth. */
            set_motion(FORWARD);
            delay(8000);
            

            set_motion(STOP);
            movement_done = 1;
            set_color(RGB(1,1,1));   // white: movement finished
            return;
        }
    }

    /* Once finished, stay stopped. */
    if (movement_done == 1)
    {
        set_motion(STOP);
    }
}

int main()
{
    kilo_init();
    kilo_start(setup, loop);
    return 0;
}
