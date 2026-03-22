#include "config.h"

extern uint8_t menu_idx;

/* Карта символов для 7-сегментного индикатора (Общий Анод).
   Бит '0' зажигает сегмент. Раскладка зависит от физической трассировки дорожек платы. */
const uint8_t seg_map[] = {
    0xC0, 0xF9, 0xA4, 0xB0, 0x99, 0x92, 0x82, 0xF8, 0x80, 0x90, // 0-9
    0xFF, // 10: Пусто (пробел)
    0x88, // 11: A (Амперы)
    0x8E, // 12: F (Front / Freq)
    0x86, // 13: E (End)
    0x92, // 14: S (Start / Slope)
    0xC2, // 15: G (Gas)
    0xC1, // 16: u (Micro)
    0xAF, // 17: r (PRe)
    0xAB, // 18: n (DowN)
    0x87, // 19: t (Time)
    0x8C, // 20: P (Pulse / Pause)
    0x98, // 21: q (freQ)
    0xA7, // 22: c (AC)
    0xA1, // 23: d (DC)
    0xC7, // 24: L (coLd)
    0xA3, // 25: o (pOst)
    0x83, // 26: b (Balance)
    0xA7  // 27: s (Меньшего размера, для разделения долей секунд)
};

void Display_Init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN | RCC_APB2ENR_AFIOEN;
    AFIO->MAPR |= AFIO_MAPR_SWJ_CFG_JTAGDISABLE;

    /* Настройка GPIO для дисплея.
       Сложная битовая маска применяется для того, чтобы случайно не сбить
       настройки пинов PB0 (Обратная связь) и PB11 (Управление мостом). */
    GPIOB->CRL = (GPIOB->CRL & 0x00000FFF) | 0x33333000;
    GPIOB->CRH = (GPIOB->CRH & 0x0000F000) | 0x33330033;
}

/* Динамическая развертка (вызывается таймером 1 раз в мс) */
void Display_Refresh_Step(void) {
    /* Гашение общих анодов (PB12-PB15) перед сменой сегментов.
       Предотвращает эффект "двоения" или слабой засветки соседних разрядов. */
    GPIOB->BRR = (0xF << 12);

    /* Декодирование символа */
    uint8_t d = display_buf[cur_digit];
    if (d > 27) d = 10;
    uint8_t code = seg_map[d];

    /* Атомарная установка/сброс состояний пинов через BSRR.
       Это самый надежный способ управления портами в прерывании. */
    for (int i = 0; i < 5; i++) {
        if (code & (1 << i)) GPIOB->BSRR = (1 << (3 + i));
        else                 GPIOB->BRR  = (1 << (3 + i));
    }
    if (code & (1 << 5)) GPIOB->BSRR = (1 << 8); else GPIOB->BRR = (1 << 8);
    if (code & (1 << 6)) GPIOB->BSRR = (1 << 9); else GPIOB->BRR = (1 << 9);

    /* Включение анода для прорисованного разряда */
    GPIOB->BSRR = (1 << (12 + cur_digit));

    cur_digit = (cur_digit + 1) % 4; // Переход к следующему разряду
}

/* Подготовка буфера для отображения названия режима (например: FE, SS, AL) */
void set_display_mode_name(uint8_t mode) {
    switch(mode) {
        case 0: display_buf[0]=12; display_buf[1]=13; display_buf[2]=17; display_buf[3]=10; break;
        case 1: display_buf[0]=14; display_buf[1]=14; display_buf[2]=10; display_buf[3]=10; break;
        case 2: display_buf[0]=22; display_buf[1]=25; display_buf[2]=24; display_buf[3]=23; break;
        case 3: display_buf[0]=14; display_buf[1]=20; display_buf[2]=25; display_buf[3]=19; break;
        case 4: display_buf[0]=11; display_buf[1]=24; display_buf[2]=16; display_buf[3]=10; break;
    }
    mode_show_timer = ms_ticks + 1500; // Текст висит на экране 1.5 секунды
}

/* Формирование числовых значений для дисплея с удалением ведущих нулей */
void update_display_numbers(int32_t val) {
    if (ms_ticks < mode_show_timer) return; // Блокировка вывода, если на экране висит текст
    if (val < 0) val = 0;

    for(int i=0; i<4; i++) display_buf[i] = 10; // Очистка (гашение) экрана

    if (ui_mode == UI_WORK) {
        /* Главный экран: Амперы. Формат "A120" */
        display_buf[0] = 11;
        if (val >= 100) display_buf[1] = (val / 100) % 10;
        display_buf[2] = (val / 10) % 10;
        display_buf[3] = val % 10;
    }
    else {
        /* Экран настроек: Интеллектуальное форматирование времени */
        if (menu_idx == 0 || menu_idx == 2 || menu_idx == 3 || menu_idx == 4) {
            /* Форматирование вида 2s5 (2.5 секунды) */
            int32_t total_dec = val / 100;
            int32_t sec = total_dec / 10;
            int32_t dec = total_dec % 10;

            if (sec >= 10) {
                display_buf[0] = (sec / 10) % 10;
                display_buf[1] = sec % 10;
            } else {
                display_buf[1] = sec % 10;
            }
            display_buf[2] = 27; /* Значок разделителя 's' */
            display_buf[3] = dec;
        }
        else {
            /* Базовое форматирование для Гц, % и Ампер */
            if (val >= 1000) display_buf[0] = (val / 1000) % 10;
            if (val >= 100)  display_buf[1] = (val / 100) % 10;
            if (val >= 10)   display_buf[2] = (val / 10) % 10;
            display_buf[3] = val % 10;
        }
    }
}

/* Интерпретатор ASCII строк в символы дисплея для вывода пунктов меню */
void set_display_text(const char* txt) {
    if (ms_ticks < mode_show_timer) return;
    for (int i = 0; i < 4; i++) {
        if (txt[i] == '\0') {
            while(i < 4) display_buf[i++] = 10;
            break;
        }
        char c = txt[i];
        if (c >= '0' && c <= '9') display_buf[i] = c - '0';
        else switch(c) {
            case 'A': display_buf[i] = 11; break;
            case 'F': display_buf[i] = 12; break;
            case 'E': display_buf[i] = 13; break;
            case 'S': case 's': case '5': display_buf[i] = 14; break;
            case 'G': display_buf[i] = 15; break;
            case 'u': case 'U': case 'v': case 'V': display_buf[i] = 16; break;
            case 'r': display_buf[i] = 17; break;
            case 'n': display_buf[i] = 18; break;
            case 't': display_buf[i] = 19; break;
            case 'P': display_buf[i] = 20; break;
            case 'q': display_buf[i] = 21; break;
            case 'c': case 'C': display_buf[i] = 22; break;
            case 'd': display_buf[i] = 23; break;
            case 'L': display_buf[i] = 24; break;
            case 'o': display_buf[i] = 25; break;
            case 'b': display_buf[i] = 26; break;
            default:  display_buf[i] = 10; break;
        }
    }
}
