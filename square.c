#include "kilolib.h"

#define STOP    0
#define FORWARD 1
#define LEFT    2
#define RIGHT   3

uint8_t cur_motion = STOP;
uint8_t kilo_made_it = 0;

void set_motion(uint8_t new_motion)
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

void setup()
{
    kilo_made_it = 0;

    if (kilo_uid == 2) {
        set_color(RGB(1,0,1));   // kilobot 2 starts purple
    }

    if (kilo_uid == 3) {
        set_color(RGB(0,0,1));   // kilobot 3 starts blue
    }

    if (kilo_uid != 2 && kilo_uid != 3) {
        set_color(RGB(1,0,0));   // all other kilobots red
    }
}

void loop()
{
    if (kilo_made_it == 0)
    {
        if (kilo_uid == 2)
        {
            set_motion(FORWARD);
            delay(8000);

            set_motion(LEFT);
            delay(4000);


            set_motion(FORWARD);
            delay(7500);

            set_motion(RIGHT);
            delay(2000);


            set_motion(FORWARD);
            delay(2000);
            
            set_motion(STOP);
            kilo_made_it = 1;
            set_color(RGB(1,1,1));
        }

        if (kilo_uid == 3)
        {
            set_motion(FORWARD);
            delay(9000);

            set_motion(RIGHT);
            delay(1000);

            set_motion(FORWARD);
            delay(8000);

            set_motion(LEFT);
            delay(1000);

            set_motion(STOP);
            kilo_made_it = 1;
            set_color(RGB(1,1,1));
        }

        if (kilo_uid != 2 && kilo_uid != 3)
        {
            set_motion(STOP);
        }
    }

    if (kilo_made_it == 1)
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
