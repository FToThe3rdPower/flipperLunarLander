#include <flipper.h>
#include "lunar.h" // Include the header file for Lunar Lander

int main() {
    // Initialize the game
    init_lunar_lander();

    while (1) {
        // Main game loop
        furi_delay_ms(1000 / 30); // 30 FPS
        handle_input();
        update_lunar_lander();
        draw_lunar_lander();
    }

    return 0;
}

