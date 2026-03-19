#include "config.h"

// Глобальные переменные процесса
volatile state_t machine_state = IDLE;
volatile uint8_t cur_mode = 0;
volatile uint32_t ms_ticks = 0, state_timer = 0, mode_show_timer = 0;
volatile int32_t set_amp = 120, real_amp = 0, adc_raw = 0;
volatile uint8_t cur_digit = 0, display_buf[4] = {10, 10, 10, 10};

// Переменные для сохранения
uint32_t save_timer = 0;
bool need_save = false;

// Настройки режимов {Предгаз, Постгаз, Пульс?, Работа_мс, Пауза_мс, AC?}
WeldParams modes[] = {
    {300, 3000, false, 0, 0, false},   // 0: FE (Сталь DC)
    {300, 5000, true, 200, 200, false},// 1: SS (Нержавейка Pulse)
    {200, 2000, true, 40, 600, false}, // 2: COLD (Холодная сварка)
    {300, 2000, true, 1200, 0, false}, // 3: SPOT (Точечная - 1.2 сек)
    {400, 4000, false, 0, 0, true}     // 4: AL (Алюминий AC)
};

// --- ПРЕРЫВАНИЯ ---

void TIM1_UP_IRQHandler(void) {
    if (TIM1->SR & TIM_SR_UIF) {
        TIM1->SR &= ~TIM_SR_UIF;
        // ШИМ разрешен только во время работы или поджига
        bool is_active = (machine_state == WELD_WORK || machine_state == START_HF);
        PWM_SetUpdate(is_active);
    }
}

void TIM3_IRQHandler(void) {
    if (TIM3->SR & TIM_SR_UIF) {
        TIM3->SR &= ~TIM_SR_UIF;
        ms_ticks++;

        // Чтение АЦП (PA4)
        if (ADC1->SR & ADC_SR_EOC) {
            adc_raw = (adc_raw * 15 + ADC1->DR) / 16;
            real_amp = (adc_raw * 1000) / 4095;
        }
        ADC1->CR2 |= ADC_CR2_SWSTART;

        Welder_Process();

        // Динамическая индикация
        int32_t val = (machine_state == IDLE || machine_state == POST_GAS) ? set_amp : real_amp;
        update_display_numbers(val);
        Display_Refresh_Step();
    }
}

// --- ИНИЦИАЛИЗАЦИЯ ---

void Clock_Init(void) {
    RCC->CR |= RCC_CR_HSEON; while(!(RCC->CR & RCC_CR_HSERDY));
    FLASH->ACR |= FLASH_ACR_LATENCY_2;
    RCC->CFGR |= RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL9;
    RCC->CR |= RCC_CR_PLLON; while(!(RCC->CR & RCC_CR_PLLRDY));
    RCC->CFGR |= RCC_CFGR_SW_PLL; while((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
}

// --- ОСНОВНОЙ ЦИКЛ ---

int main(void) {
    Clock_Init();

    // Загружаем настройки из Flash перед запуском периферии
    Settings_Load();

    Display_Init();
    PWM_Init();

    // ADC1 Setup
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    ADC1->CR2 |= ADC_CR2_ADON;
    for(volatile int i=0; i<1000; i++);
    ADC1->CR2 |= ADC_CR2_CAL; while(ADC1->CR2 & ADC_CR2_CAL);
    ADC1->SQR3 = 4; // PA4

    // TIM3 Setup (1ms)
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
    TIM3->PSC = 71; TIM3->ARR = 1000;
    TIM3->DIER |= TIM_DIER_UIE;
    NVIC_EnableIRQ(TIM3_IRQn);
    TIM3->CR1 |= TIM_CR1_CEN;

    uint8_t last_enc = (GPIOA->IDR & 0x06) >> 1;
    set_display_mode_name(cur_mode);

    while (1) {
        // 1. Логика энкодера (PA1, PA2)
        uint8_t enc = (GPIOA->IDR & 0x06) >> 1;
        if (enc != last_enc) {
            if (last_enc == 0 && enc == 1) set_amp += 5;
            if (last_enc == 0 && enc == 2) set_amp -= 5;

            if (set_amp > 250) set_amp = 250;
            if (set_amp < 10) set_amp = 10;

            last_enc = enc;
            need_save = true;
            save_timer = ms_ticks + 2000; // Запись через 2 сек
        }

        // 2. Кнопка режимов (PA3)
        static uint32_t btn_t = 0;
        if (!(GPIOA->IDR & (1 << 3))) {
            if (btn_t == 0) btn_t = ms_ticks;
        } else if (btn_t > 0) {
            if (ms_ticks - btn_t > 50 && ms_ticks - btn_t < 600) {
                cur_mode = (cur_mode + 1) % 5;
                set_display_mode_name(cur_mode);

                need_save = true;
                save_timer = ms_ticks + 2000;
            }
            btn_t = 0;
        }

        // 3. Механизм сохранения (только в простое!)
        if (need_save && ms_ticks > save_timer) {
            if (machine_state == IDLE) {
                Settings_Save();
                need_save = false;
            } else {
                // Если варим - откладываем, пока не закончим
                save_timer = ms_ticks + 2000;
            }
        }
    }
}
