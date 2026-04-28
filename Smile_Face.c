#include "kilobot.h"

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
