#include <stdlib.h>
#include "stm32f0xx.h"
#include "leds.h"

//============================================================================
// Wait for n nanoseconds. (Maximum: 4.294 seconds) Credit: Rick
//============================================================================
void nano_wait(unsigned int n) {
    asm(    "        mov r0,%0\n"
            "repeat: sub r0,#83\n"
            "        bgt repeat\n" : : "r"(n) : "r0", "cc");
}

int num_leds;
int current_led;
int update_finish;
uint8_t* buffer;
Color* color_array;

static void setup_led_DMA(){
    // This function sets up DMA1 Channel 2
    RCC -> AHBENR |= RCC_AHBENR_DMA1EN;
    DMA1_Channel2 -> CPAR = &(TIM1 -> CCR1);
    DMA1_Channel2 -> CCR &= ~(DMA_CCR_PSIZE | DMA_CCR_PINC | DMA_CCR_MSIZE);
    DMA1_Channel2 -> CCR |= DMA_CCR_PSIZE_0 | DMA_CCR_CIRC | DMA_CCR_MINC | DMA_CCR_DIR | DMA_CCR_HTIE | DMA_CCR_TCIE;
    DMA1_Channel2 -> CMAR = buffer;
    DMA1_Channel2 -> CNDTR = 48; // 48 for 3 byte, 64 for 4 byte
    NVIC -> ISER[0] = 1 << DMA1_Channel2_3_IRQn;
}

static void setup_TIM1(){
    // First setup PA8 for PWM output
    RCC -> AHBENR |= RCC_AHBENR_GPIOAEN;
    GPIOA -> MODER &= ~GPIO_MODER_MODER8;
    GPIOA -> MODER |= GPIO_MODER_MODER8_1;
    GPIOA -> AFR[1] &= ~GPIO_AFRH_AFR8;
    GPIOA -> AFR[1] |= GPIO_AF_2;
    // Now setup TIM1
    RCC -> APB2ENR |= RCC_APB2ENR_TIM1EN;
    TIM1 -> BDTR |= TIM_BDTR_MOE;
    TIM1 -> PSC = 0;
    TIM1 -> ARR = 60 - 1;
    TIM1 -> CCMR1 &= ~TIM_CCMR1_OC1M;
    TIM1 -> CCMR1 |= TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1PE; // Confirm OC1PE
    TIM1 -> CCER |= TIM_CCER_CC1E;
    TIM1 -> DIER |= TIM_DIER_CC1DE;
    TIM1 -> CR1 |= TIM_CR1_CEN;
    // CCR can be changed to 19 for 0 and 38 for 1
}

static void fill_bottom_half_buffer(Color color){
    // CRITICAL FUNCTION - Follow comments when modifying!
    // Currently set to GRB. Modify line below as needed.
    uint32_t col = (color.green << 16) | (color.red << 8) | color.blue;
    for(int i = 0; i < 24; i++){
        buffer[i] = ((col & (1 << (23 - i))) >> (23 - i)) * 19 + 19;
    }
    // Comment out this for loop if using 3 byte strip.
    /*for(int i = 24; i < 32; i++){
        buffer[i] = 19;
    }*/
}

static void fill_top_half_buffer(Color color){
    // CRITICAL FUNCTION - Follow comments when modifying!
    // Currently set to GRB. Modify line below as needed.
    uint32_t col = (color.green << 16) | (color.red << 8) | color.blue;
    // Below code is for 4 byte LED strips. Comment out for 3 byte strips
    /*for(int i = 32; i < 56; i++){
        buffer[i] = ((col & (1 << (55 - i))) >> (55 -i)) * 19 + 19;
    }
    for(int i = 56; i < 64; i++){
        buffer[i] = 19;
    }*/
    // Above code is for 4 byte LED strips
    // Below code is for 3 byte LED strips. Comment out for 4 byte strips
    for(int i = 24; i < 48; i++){
        buffer[i] = ((col & (1 << (48 - i))) >> (48 - i)) * 19 + 19;
    }
    // Above code is for 3 byte LED strips
}

void enable_DMA(){
    DMA1_Channel2 -> CCR |= DMA_CCR_EN;
}

void DMA1_CH2_3_DMA2_CH1_2_IRQHandler(){
    // THIS FUNCTION IS CRITICAL - DO NOT MODIFY!
    Color curr_col = color_array[current_led % num_leds];
    if (current_led < num_leds){
        if((DMA1 -> ISR & DMA_ISR_HTIF2) == DMA_ISR_HTIF2){
            fill_bottom_half_buffer(curr_col);
            DMA1 -> IFCR |= DMA_IFCR_CHTIF2;
        } else {
            fill_top_half_buffer(curr_col);
            DMA1 -> IFCR |= DMA_IFCR_CTCIF2;
        }
        current_led += 1;
    } else if (current_led == num_leds){
       if((DMA1 -> ISR & DMA_ISR_TCIF2) != DMA_ISR_TCIF2) {
            DMA1 -> IFCR |= DMA_IFCR_CHTIF2;
            return; // Wait until next interrupt
        } else {
            DMA1 -> IFCR |= DMA_IFCR_CTCIF2;
            for(int i = 0; i < 48; i++){ // 48 for 3 byte, 64 for 4 byte
                buffer[i] = 0;
            }
        }
        nano_wait(50000);
        update_finish = 1;
        current_led += 1;
    } else {
        DMA1 -> IFCR |= DMA_IFCR_CHTIF2 | DMA_IFCR_CTCIF2;
        return;
    }
}

void fill_color(Color color){
    // fill all elements of col_arr with color
    for(int n = 0; n < num_leds; n++){
        color_array[n] = color;
    }
}

void fill_swipe_right(Color color){
    for(int i = 0; i < num_leds; i++) {
        while(!update_finish);
        color_array[i] = color;
        update_led_strip();
    }
}

void initialize_led_strip(int num){
    setup_TIM1();
    buffer = malloc(2 * 24);
    color_array = malloc(num_leds * sizeof(*color_array));
    num_leds = num;
    current_led = 0;
    update_finish = 1;
    setup_led_DMA();
    // Perform a start-up sequence?
    fill_color((Color) {.red=0x00, .green=0x00, .blue=0x00});
    update_led_strip();
}

void update_led_strip(){
    while(!update_finish);
    current_led = 0;
    update_finish = 0;
}
