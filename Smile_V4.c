#include "kilolib.h"

#define STOP    0
#define FORWARD 1
#define LEFT    2
#define RIGHT   3

#define TICKS_PER_SEC 32

#define EYE_STAY_TIME         0

#define MOUTH_SIDE_TIME       (12 * TICKS_PER_SEC)

#define MOUTH_CENTER_FWD1     (4 * TICKS_PER_SEC)
#define MOUTH_CENTER_TURN     (3 * TICKS_PER_SEC)
#define MOUTH_CENTER_FWD2     (4 * TICKS_PER_SEC)

#define NOSE_FWD1             (4 * TICKS_PER_SEC)
#define NOSE_TURN             (3 * TICKS_PER_SEC)
#define NOSE_FWD2             (6 * TICKS_PER_SEC)

uint8_t cur_motion = STOP;
uint8_t step_state = 0;
unsigned long step_start_ticks = 0;

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
    step_state = 0;
    step_start_ticks = kilo_ticks;

    if (kilo_uid == 1002 || kilo_uid == 1005) {
        set_color(RGB(0,1,0));   /* eyes = green */
    }
    else if (kilo_uid == 1004) {
        set_color(RGB(1,0,0));   /* nose = red */
    }
    else {
        set_color(RGB(0,0,1));   /* mouth = blue */
    }
}

void loop()
{
    /* Eyes stay still */
    if (kilo_uid == 1002 || kilo_uid == 1005) {
        set_motion(STOP);
        return;
    }

    /* Mouth corners: 1001 and 1006 move forward 12 s, then stop */
    if (kilo_uid == 1001 || kilo_uid == 1006) {
        switch (step_state) {

            case 0:
                set_motion(FORWARD);
                step_start_ticks = kilo_ticks;
                step_state = 1;
                break;

            case 1:
                if (kilo_ticks - step_start_ticks >= MOUTH_SIDE_TIME) {
                    set_motion(STOP);
                    step_state = 2;
                }
                break;

            case 2:
                set_motion(STOP);
                break;
        }

        return;
    }

    /* Mouth center: 1003 */
    if (kilo_uid == 1003) {
        switch (step_state) {

            case 0:
                set_motion(FORWARD);
                step_start_ticks = kilo_ticks;
                step_state = 1;
                break;

            case 1:
                if (kilo_ticks - step_start_ticks >= MOUTH_CENTER_FWD1) {
                    set_motion(RIGHT);
                    step_start_ticks = kilo_ticks;
                    step_state = 2;
                }
                break;

            case 2:
                if (kilo_ticks - step_start_ticks >= MOUTH_CENTER_TURN) {
                    set_motion(FORWARD);
                    step_start_ticks = kilo_ticks;
                    step_state = 3;
                }
                break;

            case 3:
                if (kilo_ticks - step_start_ticks >= MOUTH_CENTER_FWD2) {
                    set_motion(STOP);
                    step_state = 4;
                }
                break;

            case 4:
                set_motion(STOP);
                break;
        }

        return;
    }

    /* Nose: 1004 */
    if (kilo_uid == 1004) {
        switch (step_state) {

            case 0:
                set_motion(FORWARD);
                step_start_ticks = kilo_ticks;
                step_state = 1;
                break;

            case 1:
                if (kilo_ticks - step_start_ticks >= NOSE_FWD1) {
                    set_motion(LEFT);
                    step_start_ticks = kilo_ticks;
                    step_state = 2;
                }
                break;

            case 2:
                if (kilo_ticks - step_start_ticks >= NOSE_TURN) {
                    set_motion(FORWARD);
                    step_start_ticks = kilo_ticks;
                    step_state = 3;
                }
                break;

            case 3:
                if (kilo_ticks - step_start_ticks >= NOSE_FWD2) {
                    set_motion(STOP);
                    step_state = 4;
                }
                break;

            case 4:
                set_motion(STOP);
                break;
        }

        return;
    }
}

int main()
{
    kilo_init();
    kilo_start(setup, loop);
    return 0;
}
