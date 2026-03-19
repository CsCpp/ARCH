#include "config.h"

// Функция записи (стирает страницу и пишет 2 значения)
void Settings_Save(void) {
    FLASH->KEYR = 0x45670123; // Разблокировка Flash
    FLASH->KEYR = 0xCDEF89AB;

    // Стираем страницу
    while (FLASH->SR & FLASH_SR_BSY);
    FLASH->CR |= FLASH_CR_PER;
    FLASH->AR = FLASH_SETTINGS_ADDR;
    FLASH->CR |= FLASH_CR_STRT;
    while (FLASH->SR & FLASH_SR_BSY);
    FLASH->CR &= ~FLASH_CR_PER;

    // Пишем Magic, Amp и Mode (3 слова по 16 бит)
    FLASH->CR |= FLASH_CR_PG;

    *(__IO uint16_t*)(FLASH_SETTINGS_ADDR) = SETTINGS_MAGIC;
    while (FLASH->SR & FLASH_SR_BSY);

    *(__IO uint16_t*)(FLASH_SETTINGS_ADDR + 2) = (uint16_t)set_amp;
    while (FLASH->SR & FLASH_SR_BSY);

    *(__IO uint16_t*)(FLASH_SETTINGS_ADDR + 4) = (uint16_t)cur_mode;
    while (FLASH->SR & FLASH_SR_BSY);

    FLASH->CR &= ~FLASH_CR_PG;
    FLASH->CR |= FLASH_CR_LOCK; // Заблокировать обратно
}

// Функция загрузки при старте
void Settings_Load(void) {
    uint16_t magic = *(__IO uint16_t*)(FLASH_SETTINGS_ADDR);

    if (magic == SETTINGS_MAGIC) {
        set_amp = *(__IO uint16_t*)(FLASH_SETTINGS_ADDR + 2);
        cur_mode = *(__IO uint16_t*)(FLASH_SETTINGS_ADDR + 4);

        // Защита от кривых данных
        if (set_amp < 10 || set_amp > 250) set_amp = 120;
        if (cur_mode > 4) cur_mode = 0;
    }
}
