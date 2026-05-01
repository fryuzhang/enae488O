#include "kilolib.h"

#define ONE_SECOND 32

typedef enum{
    EDGE,
    CENTER,
} kilo_goal_t;
kilo_goal_t kilo_goal = EDGE;

typedef enum{
    STOP,
    FORWARD,
    LEFT,
    RIGHT
} motion_t;

uint32_t move_time = ONE_SECOND * 2;
motion_t cur_motion = STOP;

void set_motion(motion_t new_motion){
    if(cur_motion != new_motion){
        cur_motion = new_motion;

        switch (cur_motion)
        {
        case STOP:
            set_motors(0, 0);
            set_color(RGB(1, 0, 1)); // red
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


void setup(){
    if (kilo_uid == 1000){
        kilo_goal = CENTER;
    }
    else{
        kilo_goal = EDGE;
    }    
}

void loop(){
    if (kilo_goal == CENTER){
        if (kilo_ticks < 32 * 12){
            set_motion(FORWARD);
        } else{
            set_motion(STOP);
            set_color(RGB(1,1,1));
        }
    } else{
        set_color(RGB(0, 1, 0));
    }

}

int main(){
    kilo_init();
    kilo_start(setup, loop);
    
    return 0;
}
