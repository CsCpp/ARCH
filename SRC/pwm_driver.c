#include "config.h"

void PWM_Init(void) {
    /* 1. Инициализация тактирования периферии */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_AFIOEN | RCC_APB2ENR_TIM1EN;

    /* Освобождение пинов JTAG (для дисплея) и переназначение выходов TIM1 (Partial Remap). */
    AFIO->MAPR |= AFIO_MAPR_SWJ_CFG_JTAGDISABLE | AFIO_MAPR_TIM1_REMAP_PARTIALREMAP;

    /* 2. Настройка GPIO для силового управления */
    GPIOA->CRH = (GPIOA->CRH & ~0x000000FF) | 0x000000BB; // ШИМ пины
    GPIOA->CRL = 0x92208888;
    GPIOA->ODR |= 0x0F;

    GPIOB->CRL = (GPIOB->CRL & ~0x000000FF) | 0x0000008B;
    GPIOB->CRH = (GPIOB->CRH & ~0x0000F000) | 0x00003000; // PB11 (Блокировка моста)

    BRIDGE_STOP(); // Принудительно глушим мост до полного запуска таймера

    /* 3. Настройка таймера TIM1 (Генерация 16 кГц) */
    TIM1->PSC = 0;
    TIM1->ARR = PWM_MAX; // 4500 тактов при 72 МГц

    /* Режим Center-Aligned не используется, классический PWM Mode 1. Включена предзагрузка регистра CCR. */
    TIM1->CCMR1 = 0x6060 | TIM_CCMR1_OC1PE | TIM_CCMR1_OC2PE;
    TIM1->CCER = 0x0055; // Включение каналов 1, 1N, 2, 2N (комплементарные пары)

    /* Генератор мертвого времени (BDTR).
       Значение DEAD_TIME (160) предотвращает сквозные токи через транзисторы верхнего и нижнего плеча. */
    TIM1->BDTR = TIM_BDTR_OSSR | TIM_BDTR_OSSI | DEAD_TIME;

    /* Синхронизация теневых регистров и включение прерываний ШИМ */
    TIM1->EGR |= TIM_EGR_UG;
    TIM1->DIER |= TIM_DIER_UIE;

    /* Высший приоритет (0) гарантирует жесткий real-time для ШИМ-контроллера */
    NVIC_SetPriority(TIM1_UP_IRQn, 0);
    NVIC_EnableIRQ(TIM1_UP_IRQn);
    TIM1->CR1 |= TIM_CR1_CEN;
}

/* Функция поканального управления мостом (вызывается из прерывания TIM1_UP) */
void PWM_SetUpdate(bool active)
{
    /* АППАРАТНАЯ ЗАЩИТА: Чтение сигнала Fault от силовых драйверов (например, по перегрузке).
       В случае ошибки сварка рубится моментально, состояние FSM сбрасывается. */
    if (GPIOB->IDR & (1 << 1)) {
        active = false;
        machine_state = IDLE;
    }

    if (!active) {
        /* Безопасное аппаратное выключение. Сброс бита MOE (Main Output Enable)
           закрывает все выходы ШИМ за наносекунды. */
        TIM1->BDTR &= ~TIM_BDTR_MOE;
        BRIDGE_STOP();
        TIM1->CCR1 = 0;
        TIM1->CCR2 = 0;
        return;
    }

    /* Пропорциональный расчет скважности ШИМ (Duty Cycle) на основе целевого тока.
       Формула: (Текущий_ток * Разрешение_ШИМ) / Максимальный_ток */
    uint16_t duty = (uint16_t)((current_setpoint * PWM_MAX) / 2500);

    /* Ограничение скважности до PWM_MAX_DUTY (93%). Защищает трансформатор от насыщения,
       а транзисторы - от невозможности корректно переключиться. */
    if (duty > PWM_MAX_DUTY) duty = PWM_MAX_DUTY;

    TIM1->BDTR |= TIM_BDTR_MOE;
    BRIDGE_RUN();

    /* ГЕНЕРАЦИЯ AC / DC */
    if (modes[cur_mode].is_ac) {
        static uint16_t ac_cnt = 0;
        static uint8_t ac_side = 0;

        /* Расчет количества циклов ШИМ на один полупериод переменного тока.
           (16000 Гц ШИМ / 2) / Частота AC = циклов на полупериод. */
        uint16_t ac_threshold = 8000 / modes[cur_mode].ac_freq;

        if (++ac_cnt >= ac_threshold) {
            ac_cnt = 0;
            ac_side = !ac_side; // Перекидываем полярность
        }

        /* Коммутация "диагоналей" Н-моста. В AC TIG одно плечо работает в режиме ШИМ,
           а другое статично открыто для формирования полярности. */
        if (ac_side == 0) {
            TIM1->CCR1 = duty; TIM1->CCR2 = 0;
        } else {
            TIM1->CCR1 = 0;    TIM1->CCR2 = duty;
        }
    } else {
        /* DC TIG (Постоянный ток): Работает всегда только прямая полярность. */
        TIM1->CCR1 = duty;
        TIM1->CCR2 = 0;
    }
}
