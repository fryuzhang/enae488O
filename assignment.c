void loop(){
    // exiled bots freeze as white and do nothing else
    if (is_exiled) {
        set_color(RGB(1, 1, 1));
        return;
    }

    if(current_phase == PHASE1){

        if(new_message){
            new_message = 0;
            last_heard_tick = kilo_ticks;  // refresh timestamp on every received message
            add_dependencies();
            validate_inclusion();
        }

        remove_dependencies();

        uint8_t current_size = check_global_size();

        if (current_size >= TOTAL_NUM) {
            if (full_size_start_tick == 0) {
                full_size_start_tick = kilo_ticks;
            }
            if ((kilo_ticks - full_size_start_tick) >= REVOLUTION_START_TIME) {
                revolution = 1;
            }
        }

        update_message();
        update_color(current_size);

        // if isolated after revolution, self-exile with yellow
        if (revolution == 1 && last_heard_tick != 0 &&
            (kilo_ticks - last_heard_tick) >= ISOLATION_TIMEOUT) {
            set_color(RGB(1, 1, 0)); // yellow — I am isolated
            is_exiled = 1;
        }

        // if(revolution == 1 && current_size == 1){
        //     current_phase = PHASE2;
        // }

    } else { // phase 2

    }
}
