#include "kilobot.h"

// Movement Motors
// declare constants
#define STOP 0
#define FORWARD 1
#define LEFT 2
#define RIGHT 3

// declare variables
uint8_t cur_motion = STOP;
uint8_t new_message = 0;
message_t msg;

// function to set new motion
void set_motion(uint8_t new_motion) {
    if (cur_motion != new_motion) {
        cur_motion = new_motion;
        switch(cur_motion) {
            case STOP:
                set_motors(0,0);
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


// order set up is (444,111,555,333,222,666)

void setup() {
    if (kilo_uid == 111) {
        set_color(RGB(0, 0, 3)); // Eyes-BLUE
    } else if (kilo_uid == 222) {
        set_color(RGB(0, 0, 3)); // Eyes-BLUE
    } else if (kilo_uid == 333) {
        set_color(RGB(3, 0, 3)); // Nose-MAGENTA
    } else if (kilo_uid == 444) {
        set_color(RGB(3, 0, 0)); // Mouth-RED
    } else if (kilo_uid == 555) {
        set_color(RGB(3, 0, 0)); // Mouth-RED
    } else if (kilo_uid == 666) {
        set_color(RGB(3, 0, 0)); // Mouth-RED
    } else {
        set_color(RGB(1, 1, 1)); // WHITE
    }

}
movement (){ if (kilo_uid == 111) {
        
    }

    else if (kilo_uid == 222) {
        
    }

    else if (kilo_uid == 333) {
        
    }

    else if (kilo_uid == 444) {
		delay(500);
		set_color(RGB(1, 1, 1));
		delay(500);
		set_color(RGB(3, 0, 0));    }

    else if (kilo_uid == 555) {
       
    }

    else if (kilo_uid == 666) {
		delay(500);
		set_color(RGB(1, 1, 1));
		delay(500);
		set_color(RGB(3, 0, 0));  
    }
}
void loop() {
   
}

int main() {
    kilo_init();
    kilo_start(setup, loop);
    return 0;
}
