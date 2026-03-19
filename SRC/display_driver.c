#include "config.h"

// Карта сегментов (0-9, Пусто, буквы A, F, E, S, C, O, L, D, T, P)
// Для общего катода (0 - горит, 1 - не горит, или наоборот в зависимости от схемы)
const uint8_t seg_map[] = {
    0x40, 0x79, 0x24, 0x30, 0x19, 0x12, 0x02, 0x78, 0x00, 0x10, // 0-9
    0x7F, // 10: Пусто (все выключены)
    0x08, 0x0E, 0x06, 0x12, 0xC6, 0xC0, 0xC7, 0xA1, 0x07, 0x0C  // 11-20: A, F, E, S, C, O, L, D, T, P
};

/**
 * Инициализация дисплея.
 */
void Display_Init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN | RCC_APB2ENR_AFIOEN;

    // Отключаем JTAG для освобождения PB3 и PB4
    AFIO->MAPR |= AFIO_MAPR_SWJ_CFG_JTAGDISABLE;

    // Настройка PB3-PB7 (CRL) как Output Push-Pull 50MHz
    GPIOB->CRL &= ~0xFFFFF000;
    GPIOB->CRL |=  0x33333000;

    // Настройка PB8, PB9 и PB12-PB15 (CRH) как Output 50MHz
    GPIOB->CRH &= ~0xFFFF0F33;
    GPIOB->CRH |=  0x33330033;
}

/**
 * Шаг динамической индикации (вызывать в TIM3 каждые 1-2 мс)
 */
void Display_Refresh_Step(void) {
    // 1. Гасим все аноды (PB12-PB15)
    GPIOB->BRR = (0xF << 12);

    // 2. Получаем символ для текущего разряда из буфера
    uint8_t d = display_buf[cur_digit];
    if (d > 20) d = 10; // Защита от выхода за пределы массива

    // 3. Вывод сегментов на PB3-PB9 без затирки PB0 (PWM)
    uint32_t odr_temp = GPIOB->ODR;
    odr_temp &= ~(0x7F << 3);                // Очищаем биты PB3-PB9
    odr_temp |= (uint32_t)(seg_map[d] << 3); // Записываем код сегмента из карты
    GPIOB->ODR = odr_temp;

    // 4. Включаем анод текущего разряда
    GPIOB->BSRR = (1 << (12 + cur_digit));

    // 5. Переходим к следующему разряду
    cur_digit = (cur_digit + 1) % 4;
}

/**
 * Установка названия режима (на 1.5 секунды)
 */
void set_display_mode_name(uint8_t mode) {
    switch(mode) {
        case 0: display_buf[0]=12; display_buf[1]=13; break; // FE
        case 1: display_buf[0]=14; display_buf[1]=14; break; // SS
        case 2: display_buf[0]=15; display_buf[1]=16; break; // CO (Cold)
        case 3: display_buf[0]=14; display_buf[1]=20; break; // SP (Spot)
        case 4: display_buf[0]=11; display_buf[1]=17; break; // AL (Alu)
    }
    display_buf[2]=10;
    display_buf[3]=10;

    mode_show_timer = ms_ticks + 1500;
}

/**
 * Обновление цифр (ток или параметры меню)
 */
void update_display_numbers(int32_t val) {
    if (ms_ticks < mode_show_timer) return;

    // Если значение отрицательное (мало ли), показываем 0
    if (val < 0) val = 0;

    // Разделение на разряды
    if (val >= 100) {
        display_buf[1] = (uint8_t)((val / 100) % 10);
    } else {
        display_buf[1] = 10; // Гасим сотни, если число меньше 100
    }

    display_buf[2] = (uint8_t)((val / 10) % 10);
    display_buf[3] = (uint8_t)(val % 10);

    // В первом разряде всегда 'A' (индекс 11) для рабочего режима
    display_buf[0] = 11;
}

/**
 * Вывод текстовых меток (Pr G, SAvE, ESc и т.д.)
 * Именно этой функции не хватало линковщику!
 */
void set_display_text(const char* txt) {
    if (ms_ticks < mode_show_timer) return;

    for (int i = 0; i < 4; i++) {
        if (txt[i] == '\0') {
            while(i < 4) display_buf[i++] = 10;
            break;
        }

        char c = txt[i];
        // Сопоставление букв из меню с индексами seg_map
        if (c >= '0' && c <= '9') display_buf[i] = c - '0';
        else if (c == ' ') display_buf[i] = 10;
        else if (c == 'P') display_buf[i] = 20;
        else if (c == 'r') display_buf[i] = 17; // 'r' через 'L'
        else if (c == 'G') display_buf[i] = 15; // 'G' через 'C'
        else if (c == 'S' || c == '5') display_buf[i] = 14;
        else if (c == 't') display_buf[i] = 19;
        else if (c == 'A') display_buf[i] = 11;
        else if (c == 'U') display_buf[i] = 16;
        else if (c == 'n') display_buf[i] = 11;
        else if (c == 'E') display_buf[i] = 13;
        else if (c == 'c') display_buf[i] = 15;
        else if (c == 'b') display_buf[i] = 18; // 'b' через 'd'
        else if (c == 'L') display_buf[i] = 17;
        else if (c == 'F') display_buf[i] = 12;
        else display_buf[i] = 10;
    }
}
