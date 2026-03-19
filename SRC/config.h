#ifndef CONFIG_H_
#define CONFIG_H_

#include "stm32f1xx.h"
#include <stdbool.h>

#define PWM_MAX      1000
#define DEAD_TIME    72
#define AC_HALF_PERIOD 10
#define FLASH_SETTINGS_ADDR  0x0800FC00  // Адрес последней страницы (для 64Кб версии)
#define SETTINGS_MAGIC       0xABCD      // Метка, что память не пустая

// Пины управления
#define GAS_ON()         (GPIOA->BSRR = GPIO_BSRR_BS5)
#define GAS_OFF()        (GPIOA->BRR  = GPIO_BRR_BR5)
#define OSC_ON()         (GPIOA->BSRR = GPIO_BSRR_BS6) // PA6 теперь осциллятор
#define OSC_OFF()        (GPIOA->BRR  = GPIO_BRR_BR6)
#define BRIDGE_RUN()     (GPIOB->BRR  = GPIO_BSRR_BS11) // 0 = Run (Buffer OE)
#define BRIDGE_STOP()    (GPIOB->BSRR = GPIO_BSRR_BS11) // 1 = Stop

typedef enum { IDLE, PRE_GAS, START_HF, WELD_WORK, WELD_PAUSE, POST_GAS } state_t;

typedef struct {
    uint32_t pre_gas, post_gas;
    bool is_pulse;
    uint32_t p_work, p_pause;
    bool is_ac;
} WeldParams;

extern WeldParams modes[];
extern volatile state_t machine_state;
extern volatile uint8_t cur_mode;
extern volatile uint32_t ms_ticks, state_timer, mode_show_timer;
extern volatile int32_t set_amp, real_amp, adc_raw;
extern volatile uint8_t cur_digit, display_buf[4];

void PWM_Init(void);
void PWM_SetUpdate(bool active);
void Display_Init(void);
void Display_Refresh_Step(void);
void set_display_mode_name(uint8_t mode);
void update_display_numbers(int32_t val);
void Welder_Process(void);
void Settings_Save(void);
void Settings_Load(void);

#endif
