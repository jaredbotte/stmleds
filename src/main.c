#include "stm32f0xx.h"
#include "leds.h"

extern Color* color_array;

int main(void)
{
    initialize_led_strip(145);
    enable_DMA();
	for(;;){
	    fill_color((Color) {.red=255, .blue=0, .green=0});
	    update_led_strip();
	    nano_wait(1000000000);
	    fill_color((Color) {.red=0, .blue=0, .green=255});
	    update_led_strip();
	    nano_wait(1000000000);
	    fill_color((Color) {.red=0, .blue=255, .green=255});
	    update_led_strip();
	    nano_wait(1000000000);
	}
}
