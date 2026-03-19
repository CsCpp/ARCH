#include "config.h"

void PWM_Init(void) {
    // Включаем тактирование портов и таймера
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_AFIOEN | RCC_APB2ENR_TIM1EN;

    // Ремап и освобождение пинов JTAG для дисплея
    AFIO->MAPR |= AFIO_MAPR_SWJ_CFG_JTAGDISABLE | AFIO_MAPR_TIM1_REMAP_PARTIALREMAP;

    // Настройка GPIOA: PA8,9 (Верхние AF_PP), PA7 (Нижний 1N AF_PP), PA0-3 (Входы), PA4 (Аналог), PA5-6 (Выходы)
    GPIOA->CRH = (GPIOA->CRH & ~0x000000FF) | 0x000000BB;
    GPIOA->CRL = 0x92208888;
    GPIOA->ODR |= 0x0F; // Подтяжка кнопок PA0-PA3

    // Настройка GPIOB: PB0 (Нижний 2N AF_PP), PB1 (Вход защиты)
    // Остальные пины PB3-PB15 настраиваются в display_driver, здесь только фиксируем PB0/PB1
    GPIOB->CRL = (GPIOB->CRL & ~0x000000FF) | 0x0000008B;

    // PB11 (Buffer OE) - Настройка как выход
    GPIOB->CRH = (GPIOB->CRH & ~0x0000F000) | 0x00003000;

    BRIDGE_STOP(); // Закрываем буфер (PB11 = 1)

    // Настройка TIM1 (16 кГц)
    TIM1->PSC = 0;
    TIM1->ARR = 4500;
    TIM1->CCMR1 = 0x6060 | TIM_CCMR1_OC1PE | TIM_CCMR1_OC2PE;

    // Включаем все 4 выхода: CC1E (PA8), CC1NE (PA7), CC2E (PA9), CC2NE (PB0)
    TIM1->CCER = 0x0055;

    // Deadtime и безопасные состояния при стопе
    TIM1->BDTR = TIM_BDTR_OSSR | TIM_BDTR_OSSI | 160;

    TIM1->EGR |= TIM_EGR_UG; // Принудительное обновление регистров
    TIM1->DIER |= TIM_DIER_UIE;
    NVIC_SetPriority(TIM1_UP_IRQn, 0);
    NVIC_EnableIRQ(TIM1_UP_IRQn);
    TIM1->CR1 |= TIM_CR1_CEN;
}

void PWM_SetUpdate(bool active) {
    // 1. Аппаратная защита по входу PB1
    if (GPIOB->IDR & (1 << 1)) {
        active = false;
        machine_state = IDLE;
    }

    // 2. Если сварка НЕ активна (Пауза в пульсе или Конец SPOT)
    if (!active) {
        TIM1->BDTR &= ~TIM_BDTR_MOE; // Гасим ключи
        BRIDGE_STOP();               // Закрываем буфер (PB11=1)
        TIM1->CCR1 = 0;              // Скважность в ноль
        TIM1->CCR2 = 0;
        return;
    }

    // 3. Активная фаза сварки
    uint16_t duty = (set_amp * 4500) / 250;

    TIM1->BDTR |= TIM_BDTR_MOE; // Разрешаем работу моста
    BRIDGE_RUN();               // Открываем буфер (PB11=0)

    // Логика AC/DC
    if (modes[cur_mode].is_ac) {
        static uint16_t ac_cnt = 0;
        static uint8_t ac_side = 0;
        if (++ac_cnt >= 160) { ac_cnt = 0; ac_side = !ac_side; }

        if (ac_side == 0) {
            TIM1->CCR1 = duty; TIM1->CCR2 = 0;
        } else {
            TIM1->CCR1 = 0;    TIM1->CCR2 = duty;
        }
    } else {
        // Обычный DC режим (используем только первое плечо моста)
        TIM1->CCR1 = duty;
        TIM1->CCR2 = 0;
    }
}
