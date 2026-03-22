#include "config.h"
#include <string.h>

#define MODES_SIZE_BYTES (sizeof(WeldParams) * 5)

/* Сохранение параметров во встроенную Flash-память контроллера с защитой от износа. */
void Settings_Save(void) {
    uint16_t *flash_ptr = (uint16_t*)FLASH_SETTINGS_ADDR;

    /* 1. ПРОВЕРКА ЦЕЛЕСООБРАЗНОСТИ ЗАПИСИ (Wear-Leveling)
       Стирание страницы Flash — долгий и деструктивный процесс.
       Сначала проверяем, отличаются ли текущие данные от тех, что уже записаны. */
    bool data_changed = false;
    if (flash_ptr[0] != SETTINGS_MAGIC) data_changed = true;
    if (flash_ptr[1] != (uint16_t)cur_mode) data_changed = true;
    if (flash_ptr[2] != (uint16_t)set_amp) data_changed = true;

    /* Быстрое побайтовое сравнение всего массива структуры режимов */
    if (!data_changed) {
        if (memcmp(&flash_ptr[3], (void*)modes, MODES_SIZE_BYTES) != 0) {
            data_changed = true;
        }
    }

    if (!data_changed) return; // Настройки идентичны — выходим без изменения памяти

    /* 2. АППАРАТНАЯ ЗАПИСЬ (Стирание страницы и прошивка слов) */

    /* Снятие блокировки (Unlock) интерфейса Flash с помощью специальных ключей ST. */
    FLASH->KEYR = 0x45670123;
    FLASH->KEYR = 0xCDEF89AB;

    while (FLASH->SR & FLASH_SR_BSY); // Ожидание готовности контроллера

    /* Инициация стирания страницы (Page Erase).
       В STM32 нельзя переписать бит с 0 на 1 без полного стирания страницы в 1 КБ. */
    FLASH->CR |= FLASH_CR_PER;
    FLASH->AR = FLASH_SETTINGS_ADDR;
    FLASH->CR |= FLASH_CR_STRT;
    while (FLASH->SR & FLASH_SR_BSY);
    FLASH->CR &= ~FLASH_CR_PER;

    /* Перевод контроллера в режим программирования (Programming) */
    FLASH->CR |= FLASH_CR_PG;
    uint16_t *dest = (uint16_t*)FLASH_SETTINGS_ADDR;

    /* Сохранение заголовка (Магический ключ, активный режим, базовый ток) */
    *dest++ = SETTINGS_MAGIC;
    while (FLASH->SR & FLASH_SR_BSY);
    *dest++ = (uint16_t)cur_mode;
    while (FLASH->SR & FLASH_SR_BSY);
    *dest++ = (uint16_t)set_amp;
    while (FLASH->SR & FLASH_SR_BSY);

    /* Копирование массива настроек из оперативной памяти в энергонезависимую.
       Запись ведется 16-битными словами (Half-Word). */
    uint16_t *src = (uint16_t*)modes;
    for (uint16_t i = 0; i < (MODES_SIZE_BYTES / 2); i++) {
        *dest++ = *src++;
        while (FLASH->SR & FLASH_SR_BSY);
    }

    /* Возврат блокировки Flash для предотвращения случайного стирания при зависаниях. */
    FLASH->CR &= ~FLASH_CR_PG;
    FLASH->CR |= FLASH_CR_LOCK;
}

/* Загрузка и проверка валидности параметров при включении питания аппарата. */
void Settings_Load(void) {
    uint16_t *src = (uint16_t*)FLASH_SETTINGS_ADDR;
    bool needs_reinit = false;

    /* Проверка ключа. Если память чистая (0xFFFF) или содержит мусор,
       оставляем заводские значения и форсируем сохранение. */
    if (*src != SETTINGS_MAGIC) {
        needs_reinit = true;
    } else {
        src++;
        uint16_t loaded_mode = *src++;
        uint16_t loaded_amp  = *src++;

        /* Защита от битых значений (например, при прерванной записи во время скачка света). */
        if (loaded_mode > 4) { loaded_mode = 0; needs_reinit = true; }
        if (loaded_amp < 10 || loaded_amp > 250) { loaded_amp = 80; needs_reinit = true; }

        cur_mode = (uint8_t)loaded_mode;
        set_amp = (int32_t)loaded_amp;

        uint16_t *flash_ptr = src;

        /* Чтение и восстановление 5 рабочих пресетов. */
        for (int i = 0; i < 5; i++) {
            WeldParams temp;
            uint16_t *t_ptr = (uint16_t*)&temp;

            for(int w=0; w < sizeof(WeldParams)/2; w++) {
                *t_ptr++ = *flash_ptr++;
            }

            /* Жесткая валидация критических диапазонов перед загрузкой в ОЗУ.
               Если значение выходит за рамки физики процесса — сбрасываем на дефолт. */
            if (temp.pre_gas > 2000)   { temp.pre_gas = 300;  needs_reinit = true; }
            if (temp.post_gas > 20000) { temp.post_gas = 3000; needs_reinit = true; }
            if (temp.start_amp > 100)  { temp.start_amp = 40;  needs_reinit = true; }
            if (temp.ac_freq > 200)    { temp.ac_freq = 100;   needs_reinit = true; }
            if (temp.ac_balance > 50)  { temp.ac_balance = 30; needs_reinit = true; }

            modes[i] = temp;
        }
    }

    /* Если была найдена хоть одна ошибка или это первый запуск —
       перезаписываем память корректными данными. */
    if (needs_reinit) {
        Settings_Save();
    }
}
