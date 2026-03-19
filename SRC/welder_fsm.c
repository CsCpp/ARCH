#include "config.h"

void Welder_Process(void) {
    // PA0 - Кнопка горелки (Активный низкий уровень, поэтому инвертируем)
    bool btn = !(GPIOA->IDR & (1 << 0));

    switch (machine_state) {
        case IDLE:
            // Ждем нажатия кнопки
            if (btn) {
                GPIOA->BSRR = (1 << 5); // Включаем ГАЗ (PA5)
                state_timer = ms_ticks;
                machine_state = PRE_GAS;
            }
            break;

        case PRE_GAS:
            // Ждем время предгаза
            if (ms_ticks - state_timer >= modes[cur_mode].pre_gas) {
                GPIOA->BSRR = (1 << 6); // Включаем ОСЦИЛЛЯТОР (PA6)
                state_timer = ms_ticks;
                machine_state = START_HF;
            }
            // Если кнопку бросили во время предгаза — уходим на продувку
            if (!btn) machine_state = POST_GAS;
            break;

        case START_HF:
            // Время работы осциллятора (из твоей прошивки — 500мс)
            if (ms_ticks - state_timer >= 500) {
                GPIOA->BRR = (1 << 6);  // Выключаем ОСЦИЛЛЯТОР
                state_timer = ms_ticks;
                machine_state = WELD_WORK;
            }
            // Если дуга не зажглась и кнопку бросили
            if (!btn) machine_state = POST_GAS;
            break;

        case WELD_WORK:
            // Если кнопку отпустили (и это не SPOT), идем в POST_GAS
            if (!btn && cur_mode != 3) {
                state_timer = ms_ticks;
                machine_state = POST_GAS;
                break;
            }

            // Логика импульсных режимов (SS, COLD, SPOT)
            if (modes[cur_mode].is_pulse) {
                if (ms_ticks - state_timer >= modes[cur_mode].p_work) {
                    state_timer = ms_ticks;

                    if (cur_mode == 3) {
                        // РЕЖИМ SPOT: Отработали 1.2с и принудительно гасим дугу
                        machine_state = POST_GAS;
                    } else {
                        // РЕЖИМЫ SS / COLD: Переходим в фазу паузы
                        machine_state = WELD_PAUSE;
                    }
                }
            } else {
                // Обычная сварка (DC FE или AC AL): варим пока нажата кнопка
                if (!btn) machine_state = POST_GAS;
            }
            break;

        case WELD_PAUSE:
            // Если кнопку отпустили в паузе — прекращаем сварку
            if (!btn) {
                state_timer = ms_ticks;
                machine_state = POST_GAS;
                break;
            }

            // Ждем окончания паузы (например, 600мс для COLD)
            if (ms_ticks - state_timer >= modes[cur_mode].p_pause) {
                state_timer = ms_ticks;
                machine_state = WELD_WORK; // Снова даем импульс тока
            }
            break;

        case POST_GAS:
            GPIOA->BRR = (1 << 6); // На всякий случай гасим осциллятор

            // Ждем время постгаза
            if (ms_ticks - state_timer >= modes[cur_mode].post_gas) {
                // Выходим в IDLE только если кнопка ОТПУЩЕНА (защита от перезапуска)
                if (!btn) {
                    GPIOA->BRR = (1 << 5); // Выключаем ГАЗ
                    machine_state = IDLE;
                }
            }

            // Быстрый рестарт: если нажали кнопку во время продувки (кроме SPOT)
            if (btn && cur_mode != 3) {
                state_timer = ms_ticks;
                machine_state = PRE_GAS;
            }
            break;

        default:
            machine_state = IDLE;
            break;
    }
}
