#include "config.h"
#include <string.h>

/* Инициализация глобальных переменных системы */
volatile state_t machine_state = IDLE;
volatile ui_state_t ui_mode = UI_WORK;
volatile uint8_t cur_mode = 0;

/* Системное время и таймеры состояний */
volatile uint32_t ms_ticks = 0, state_timer = 0, mode_show_timer = 0;

/* Переменные управления током (x10 для сохранения дробной части при расчетах) */
volatile int32_t set_amp = 120, real_amp = 0, adc_raw = 0;

/* Буфер дисплея (10 означает пустоту/погашенный символ) */
volatile uint8_t cur_digit = 0, display_buf[4] = {10, 10, 10, 10};

/* Флаги и таймеры для механизма умного сохранения во Flash */
bool need_save = false;
static bool menu_entered = false;
static bool arc_was_present = false;

/* Пользовательский интерфейс: названия параметров меню */
const char* menu_labels[] = {
    "Pr G", "St A", "UP S", "dn S", "Po G",
    "t Ur", "t PA", "FrEq", "bALn", "5AvE", "E5c"
};
uint8_t menu_idx = 0;
#define MENU_MAX 11

/* Заводские пресеты для 5 режимов сварки (TIG DC, TIG AC, Pulse и т.д.) */
WeldParams modes[] = {
    {300, 40, 500, 2000, 3000, 0, 0, 0, 0, false, false},
    {300, 30, 300, 1500, 5000, 200, 200, 0, 0, true, false},
    {200, 60, 0, 0, 2000, 40, 600, 0, 0, true, false},
    {300, 80, 0, 0, 2000, 1200, 0, 0, 0, true, false},
    {400, 50, 1000, 3000, 4000, 0, 0, 100, 30, false, true}
};

/* -------------------------------------------------------------------------- */
/* ОБРАБОТЧИКИ ПРЕРЫВАНИЙ                                                     */
/* -------------------------------------------------------------------------- */

/* Прерывание таймера TIM1 (Генерация ШИМ).
   Срабатывает 16 000 раз в секунду. Определяет, нужно ли держать ключи открытыми. */
void TIM1_UP_IRQHandler(void) {
    if (TIM1->SR & TIM_SR_UIF) {
        TIM1->SR &= ~TIM_SR_UIF; // Сброс флага прерывания

        /* ШИМ активен во всех фазах, где присутствует сварочная дуга */
        bool is_active = (machine_state == START_HF ||
                          machine_state == WELD_WORK ||
                          machine_state == WELD_PAUSE ||
                          machine_state == DOWN_SLOPE);

        PWM_SetUpdate(is_active);
    }
}

/* Прерывание таймера TIM3 (Системный таймер).
   Срабатывает строго 1 раз в миллисекунду (1 кГц). Сердце диспетчера задач. */
void TIM3_IRQHandler(void) {
    if (TIM3->SR & TIM_SR_UIF) {
        TIM3->SR &= ~TIM_SR_UIF;
        ms_ticks++;

        /* Экспоненциальное скользящее среднее (EMA) для фильтрации шумов АЦП датчика тока.
           Усиливает влияние предыдущих значений (вес 15/16), подавляя резкие всплески от ВЧ-помех. */
        if (ADC1->SR & ADC_SR_EOC) {
            adc_raw = (adc_raw * 15 + ADC1->DR) / 16;
            real_amp = (adc_raw * 1000) / 4095;
        }
        ADC1->CR2 |= ADC_CR2_SWSTART; // Запуск следующего преобразования

        /* Обработка циклограммы сварки */
        Welder_Process();

        /* Обновление данных на экране (если не находимся в меню настроек) */
        if (ui_mode == UI_WORK) {
            int32_t val = (machine_state == IDLE || machine_state == POST_GAS) ? set_amp : real_amp;
            update_display_numbers(val);
        }

        /* Динамическая развертка одного сегмента дисплея */
        Display_Refresh_Step();
    }
}

/* -------------------------------------------------------------------------- */
/* СИСТЕМНЫЕ ФУНКЦИИ                                                          */
/* -------------------------------------------------------------------------- */

/* Инициализация тактирования от внешнего кварца на 72 МГц через PLL */
void Clock_Init(void) {
    RCC->CR |= RCC_CR_HSEON;
    while(!(RCC->CR & RCC_CR_HSERDY));
    FLASH->ACR |= FLASH_ACR_LATENCY_2; // Задержка Flash для 72 МГц
    RCC->CFGR |= RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL9; // 8 MHz * 9 = 72 MHz
    RCC->CR |= RCC_CR_PLLON;
    while(!(RCC->CR & RCC_CR_PLLRDY));
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
}

/* Безопасное изменение параметров меню (с защитой от выхода за пределы диапазонов) */
void Modify_Parameter(int8_t dir) {
    WeldParams *p = &modes[cur_mode];
    switch(menu_idx) {
        case 0:
            if (dir > 0 && p->pre_gas < 2000) p->pre_gas += 100;
            if (dir < 0 && p->pre_gas > 100)  p->pre_gas -= 100;
            break;
        case 1:
            if (dir > 0 && p->start_amp < 100) p->start_amp += 5;
            if (dir < 0 && p->start_amp > 10)  p->start_amp -= 5;
            break;
        case 2:
            if (dir > 0 && p->up_slope < 5000) p->up_slope += 100;
            if (dir < 0 && p->up_slope > 0)    p->up_slope -= 100;
            break;
        case 3:
            if (dir > 0 && p->down_slope < 10000) p->down_slope += 100;
            if (dir < 0 && p->down_slope > 0)     p->down_slope -= 100;
            break;
        case 4:
            if (dir > 0 && p->post_gas < 20000) p->post_gas += 100;
            if (dir < 0 && p->post_gas > 100)   p->post_gas -= 100;
            break;
        case 5:
            if (dir > 0 && p->p_work < 1000) p->p_work += 10;
            if (dir < 0 && p->p_work > 10)   p->p_work -= 10;
            break;
        case 6:
            if (dir > 0 && p->p_pause < 1000) p->p_pause += 10;
            if (dir < 0 && p->p_pause > 10)   p->p_pause -= 10;
            break;
        case 7:
            if (dir > 0 && p->ac_freq < 200) p->ac_freq += 5;
            if (dir < 0 && p->ac_freq > 20)  p->ac_freq -= 5;
            break;
        case 8:
            if (dir > 0 && p->ac_balance < 50) p->ac_balance += 1;
            if (dir < 0 && p->ac_balance > 10) p->ac_balance -= 1;
            break;
    }
}

/* -------------------------------------------------------------------------- */
/* ГЛАВНЫЙ ЦИКЛ                                                               */
/* -------------------------------------------------------------------------- */

int main(void) {
    Clock_Init();
    Settings_Load(); // Загрузка сохраненных режимов из Flash
    Display_Init();
    PWM_Init();

    /* Базовая настройка АЦП (ADC1) для чтения реального тока (шунта/датчика Холла) */
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    ADC1->CR2 |= ADC_CR2_ADON;
    for(volatile int i=0; i<1000; i++); // Микро-задержка для стабилизации АЦП
    ADC1->CR2 |= ADC_CR2_CAL;           // Аппаратная калибровка
    while(ADC1->CR2 & ADC_CR2_CAL);
    ADC1->SQR3 = 4;                     // Выбор канала

    /* Настройка таймера TIM3 (Системный тик 1 мс) */
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
    TIM3->PSC = 71;
    TIM3->ARR = 1000;
    TIM3->DIER |= TIM_DIER_UIE;
    NVIC_EnableIRQ(TIM3_IRQn);
    TIM3->CR1 |= TIM_CR1_CEN;

    static uint8_t last_enc = 0;
    static uint32_t btn_t = 0;

    set_display_mode_name(cur_mode);

    while (1) {
        /* --- 1. ЛОГИКА КНОПКИ ЭНКОДЕРА (Анализ коротких и длинных нажатий) --- */
        bool btn_enc = BTN_ENC_PRESSED();

        if (btn_enc) {
            if (btn_t == 0) {
                btn_t = ms_ticks;
                menu_entered = false;
            }
            /* Если кнопка удерживается более 1.2 секунды — входим в меню немедленно */
            if (!menu_entered && (ms_ticks - btn_t > 1200)) {
                ui_mode = (ui_mode == UI_WORK) ? UI_MENU_SEL : UI_WORK;
                menu_idx = 0;
                if(ui_mode == UI_WORK) set_display_mode_name(cur_mode);
                menu_entered = true;
            }
        } else {
            /* Кнопка отпущена */
            if (btn_t > 0) {
                uint32_t diff = ms_ticks - btn_t;
                /* Отработка короткого клика (антидребезг 50 мс) */
                if (!menu_entered && diff > 50) {
                    if (ui_mode == UI_WORK) {
                        cur_mode = (cur_mode + 1) % 5;
                        set_display_mode_name(cur_mode);
                        need_save = true;
                    } else if (ui_mode == UI_MENU_SEL) {
                        /* Выход из меню с принудительным сохранением или возврат в работу */
                        if (menu_idx == 9) { Settings_Save(); ui_mode = UI_WORK; }
                        else if (menu_idx == 10) ui_mode = UI_WORK;
                        else ui_mode = UI_MENU_SET;
                    } else if (ui_mode == UI_MENU_SET) {
                        ui_mode = UI_MENU_SEL;
                    }
                }
                btn_t = 0;
                menu_entered = false;
            }
        }

        /* --- 2. ОБРАБОТКА ВРАЩЕНИЯ ЭНКОДЕРА --- */
        /* Чтение 2 бит (фазы A и B) и определение направления вращения. */
        uint8_t enc = (GPIOA->IDR & 0x06) >> 1;
        if (enc != last_enc) {
            int8_t dir = (last_enc == 0 && enc == 1) ? 1 : (last_enc == 0 && enc == 2) ? -1 : 0;
            if (dir != 0) {
                if (ui_mode == UI_WORK) {
                    set_amp += dir * 5;
                    if (set_amp > 250) set_amp = 250;
                    if (set_amp < 10) set_amp = 10;
                    need_save = true;
                } else if (ui_mode == UI_MENU_SEL) {
                    menu_idx = (menu_idx + dir + MENU_MAX) % MENU_MAX;
                } else if (ui_mode == UI_MENU_SET) {
                    Modify_Parameter(dir);
                    need_save = true;
                }
            }
            last_enc = enc;
        }

        /* --- 3. ФИКСАЦИЯ СВАРОЧНОГО ПРОЦЕССА --- */
        /* Запоминаем, что дуга горела, чтобы разрешить сохранение измененных настроек */
        if (machine_state == WELD_WORK || machine_state == START_HF) {
            arc_was_present = true;
        }

        /* --- 4. УМНОЕ СОХРАНЕНИЕ ПАРАМЕТРОВ ВО FLASH --- */
        /* Экономия ресурса памяти: запись происходит только в простое,
           если настройки менялись И процесс сварки завершен (либо прошло 10 секунд). */
        if (need_save && machine_state == IDLE) {
            if (arc_was_present || (ms_ticks - state_timer > 10000)) {
                Settings_Save();
                need_save = false;
                arc_was_present = false;
            }
        }

        /* --- 5. ОБНОВЛЕНИЕ ЭКРАНА В ЗАВИСИМОСТИ ОТ РЕЖИМА UI --- */
        if (ui_mode == UI_MENU_SEL) {
            set_display_text(menu_labels[menu_idx]);
        } else if (ui_mode == UI_MENU_SET) {
            /* Моргание значения параметра при редактировании (период 250 мс) */
            if ((ms_ticks / 250) % 2) set_display_text("    ");
            else {
                WeldParams *p = &modes[cur_mode];
                uint16_t val = 0;
                switch(menu_idx) {
                    case 0: val = p->pre_gas; break;
                    case 1: val = p->start_amp; break;
                    case 2: val = p->up_slope; break;
                    case 3: val = p->down_slope; break;
                    case 4: val = p->post_gas; break;
                    case 5: val = p->p_work; break;
                    case 6: val = p->p_pause; break;
                    case 7: val = p->ac_freq; break;
                    case 8: val = p->ac_balance; break;
                }
                update_display_numbers(val);
            }
        }
    }
}
