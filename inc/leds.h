#ifndef __LEDS_H__
#define __LEDS_H__
#include "stm32f0xx.h"

typedef struct _Color {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} Color;

// Static setup functions
static void setup_led_DMA();
static void setup_TIM1();

// Static buffer-related functions. We'll make sure this are protected.
static void fill_bottom_half_buffer(Color color);
static void fill_top_half_buffer(Color color);

// Interfacing functions
void initialize_led_strip(int num);
void update_led_strip();
void fill_color(Color color);
void enable_DMA();

#endif // __LEDS_H__
